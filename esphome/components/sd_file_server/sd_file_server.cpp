#include "webdav_server.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>

namespace esphome {
namespace webdavbox {

static const char* TAG = "webdavbox_server";

void WebDavServer::setup() {
  if (!base_) {
    ESP_LOGE(TAG, "WebServer base not set");
    return;
  }

  register_webdav_handlers();
  ESP_LOGI(TAG, "WebDAV handlers registered");
}

void WebDavServer::loop() {
  // Aucune action nécessaire dans la boucle
}

void WebDavServer::register_webdav_handlers() {
  base_->add_handler([this](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_PROPFIND) {
      handle_propfind(request);
      return true;
    }
    return false;
  });

  base_->add_handler([this](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET) {
      handle_get(request);
      return true;
    }
    return false;
  });

  base_->add_handler([this](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_PUT) {
      handle_put(request);
      return true;
    }
    return false;
  });

  base_->add_handler([this](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_DELETE) {
      handle_delete(request);
      return true;
    }
    return false;
  });

  base_->add_handler([this](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_MKCOL) {
      handle_mkcol(request);
      return true;
    }
    return false;
  });
}

bool WebDavServer::authenticate_request(AsyncWebServerRequest* request) {
  // Si aucun nom d'utilisateur/mot de passe n'est défini, pas d'authentification
  if (username_.empty() || password_.empty()) {
    return true;
  }

  // Vérification de l'authentification Basic
  if (!request->hasHeader("Authorization")) {
    request->send(401, "text/plain", "Authentication required");
    return false;
  }

  String auth_header = request->header("Authorization");
  if (!auth_header.startsWith("Basic ")) {
    request->send(401, "text/plain", "Invalid authentication method");
    return false;
  }

  // TODO: Implémenter le décodage Base64 et la vérification
  return true;
}

std::string WebDavServer::resolve_sd_path(const std::string& request_path) {
  std::string full_path = sd_mount_point_;
  
  std::string clean_path = request_path;
  if (clean_path.empty() || clean_path == "/") {
    clean_path = "";
  }

  if (!clean_path.empty() && clean_path[0] == '/') {
    clean_path = clean_path.substr(1);
  }

  full_path += "/" + clean_path;
  return full_path;
}

void WebDavServer::send_webdav_response(AsyncWebServerRequest* request, 
                                         int status_code, 
                                         const std::string& content_type, 
                                         const std::string& body) {
  AsyncWebServerResponse* response = request->beginResponse(status_code, content_type.c_str(), body.c_str());
  request->send(response);
}

void WebDavServer::handle_propfind(AsyncWebServerRequest* request) {
  if (!authenticate_request(request)) {
    return;
  }

  std::string path = request->url();
  std::string full_path = resolve_sd_path(path);
  
  DIR* dir = opendir(full_path.c_str());
  if (!dir) {
    send_webdav_response(request, 404, "text/plain", "Not Found");
    return;
  }

  std::string xml_response = 
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<D:multistatus xmlns:D=\"DAV:\">\n";

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') continue;

    std::string entry_path = full_path + "/" + entry->d_name;
    struct stat path_stat;
    if (stat(entry_path.c_str(), &path_stat) == 0) {
      xml_response += 
        "  <D:response>\n"
        "    <D:href>" + path + "/" + entry->d_name + "</D:href>\n"
        "    <D:propstat>\n"
        "      <D:prop>\n"
        "        <D:resourcetype>" + 
        (S_ISDIR(path_stat.st_mode) ? "<D:collection/>" : "") + 
        "</D:resourcetype>\n"
        "        <D:getcontentlength>" + std::to_string(path_stat.st_size) + "</D:getcontentlength>\n"
        "      </D:prop>\n"
        "      <D:status>HTTP/1.1 200 OK</D:status>\n"
        "    </D:propstat>\n"
        "  </D:response>\n";
    }
  }
  closedir(dir);

  xml_response += "</D:multistatus>";
  send_webdav_response(request, 207, "application/xml", xml_response);
}

void WebDavServer::handle_get(AsyncWebServerRequest* request) {
  if (!authenticate_request(request)) {
    return;
  }

  std::string path = request->url();
  std::string full_path = resolve_sd_path(path);
  
  FILE* file = fopen(full_path.c_str(), "rb");
  if (!file) {
    send_webdav_response(request, 404, "text/plain", "File Not Found");
    return;
  }

  // Obtenir la taille du fichier
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Définir la taille du buffer en fonction de la taille du fichier
  size_t buffer_size;
  if (file_size < 1024) {
    // Petit fichier : lecture complète
    buffer_size = file_size;
  } else if (file_size < 1024 * 1024) {
    // Fichier moyen : buffer de 4 Ko
    buffer_size = 4 * 1024;
  } else {
    // Gros fichier : buffer de 32 Ko
    buffer_size = 32 * 1024;
  }

  // Créer un contexte de streaming
  struct FileStreamContext {
    FILE* file;
    long total_size;
    long bytes_sent;
    size_t buffer_size;
  };

  FileStreamContext* context = new FileStreamContext{
    file, 
    file_size, 
    0, 
    buffer_size
  };

  AsyncWebServerResponse* response = request->beginResponse(
    "application/octet-stream", 
    file_size, 
    [context](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
      // Si tous les octets ont été envoyés, libérer les ressources
      if (context->bytes_sent >= context->total_size) {
        fclose(context->file);
        delete context;
        return 0;
      }

      // Calculer la quantité de données à lire
      size_t bytes_to_read = std::min(
        maxLen, 
        std::min(
          context->buffer_size, 
          static_cast<size_t>(context->total_size - context->bytes_sent)
        )
      );

      // Lire les données
      size_t bytes_read = fread(buffer, 1, bytes_to_read, context->file);
      
      // Mettre à jour le compteur d'octets envoyés
      context->bytes_sent += bytes_read;

      // Gérer la progression et la mémoire
      if (context->bytes_sent % (1024 * 1024) == 0) {
        ESP_LOGD(TAG, "Streaming file: %ld/%ld bytes", 
                 context->bytes_sent, context->total_size);
      }

      return bytes_read;
    }
  );

  // Définir des en-têtes supplémentaires
  response->addHeader("Accept-Ranges", "bytes");
  response->addHeader("Content-Disposition", 
    "attachment; filename=\"" + std::string(strrchr(full_path.c_str(), '/') + 1) + "\"");

  request->send(response);
}

void WebDavServer::handle_put(AsyncWebServerRequest* request) {
  if (!authenticate_request(request)) {
    return;
  }

  std::string path = request->url();
  std::string full_path = resolve_sd_path(path);
  
  // Vérifier l'espace disponible
  struct statvfs stat;
  if (statvfs(sd_mount_point_.c_str(), &stat) != 0) {
    send_webdav_response(request, 507, "text/plain", "Insufficient Storage");
    return;
  }

  // Vérifier la taille du fichier à télécharger
  if (request->contentLength() > stat.f_bfree * stat.f_frsize) {
    send_webdav_response(request, 507, "text/plain", "File Too Large");
    return;
  }

  // Contexte de streaming pour l'upload
  struct FileUploadContext {
    FILE* file;
    size_t total_size;
    size_t bytes_received;
    bool upload_complete;
  };

  FileUploadContext* context = new FileUploadContext{
    fopen(full_path.c_str(), "wb"),
    request->contentLength(),
    0,
    false
  };

  if (!context->file) {
    delete context;
    send_webdav_response(request, 500, "text/plain", "Could not create file");
    return;
  }

  request->onBody([this, request, context, full_path](AsyncWebServerRequest* req, 
                                                     uint8_t* data, 
                                                     size_t len, 
                                                     size_t index, 
                                                     size_t total) {
    // Écrire les données
    size_t bytes_written = fwrite(data, 1, len, context->file);
    context->bytes_received += bytes_written;

    // Progression du téléchargement
    if (context->bytes_received % (1024 * 1024) == 0) {
      ESP_LOGD(TAG, "Uploading file: %zu/%zu bytes", 
               context->bytes_received, context->total_size);
    }

    // Vérifier si le téléchargement est terminé
    if (context->bytes_received >= context->total_size) {
      fclose(context->file);
      context->upload_complete = true;
      send_webdav_response(request, 201, "text/plain", "File Created");
      delete context;
    }
  });

  // Gestion des erreurs d'upload
  request->onError([this, context, full_path](AsyncWebServerRequest* req, int error) {
    if (context->file) {
      fclose(context->file);
      remove(full_path.c_str());
    }
    delete context;
    send_webdav_response(req, 500, "text/plain", "Upload Failed");
  });
}

void WebDavServer::handle_delete(AsyncWebServerRequest* request) {
  if (!authenticate_request(request)) {
    return;
  }

  std::string path = request->url();
  std::string full_path = resolve_sd_path(path);
  
  struct stat path_stat;
  if (stat(full_path.c_str(), &path_stat) != 0) {
    send_webdav_response(request, 404, "text/plain", "Not Found");
    return;
  }

  if (S_ISDIR(path_stat.st_mode)) {
    if (rmdir(full_path.c_str()) == 0) {
      send_webdav_response(request, 204, "text/plain", "Deleted");
    } else {
      send_webdav_response(request, 403, "text/plain", "Forbidden");
    }
  } else {
    if (remove(full_path.c_str()) == 0) {
      send_webdav_response(request, 204, "text/plain", "Deleted");
    } else {
      send_webdav_response(request, 403, "text/plain", "Forbidden");
    }
  }
}

void WebDavServer::handle_mkcol(AsyncWebServerRequest* request) {
  if (!authenticate_request(request)) {
    return;
  }

  std::string path = request->url();
  std::string full_path = resolve_sd_path(path);
  
  if (mkdir(full_path.c_str(), 0755) == 0) {
    send_webdav_response(request, 201, "text/plain", "Created");
  } else {
    if (errno == EEXIST) {
      send_webdav_response(request, 405, "text/plain", "Method Not Allowed");
    } else {
      send_webdav_response(request, 409, "text/plain", "Conflict");
    }
  }
}

} // namespace webdavbox
} // namespace esphome
