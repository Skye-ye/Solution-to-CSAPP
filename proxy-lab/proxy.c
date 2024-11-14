#include "cache.h"
#include "csapp.h"
#include "sbuf.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_PORT_LEN 6

#define NTHREADS 12
#define SBUFSIZE 16

sbuf_t sbuf; // Shared buffer for connected descriptors
cache_t cache;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static void *thread(void *vargp);
static void doit(int fd);
static void parse_url(char *url, char *host, char *port, char *path);
static void deal_request(int clientfd, char *host, char *port, char *path,
                         char *url);
static void add_to_cache(char *url, char *data, size_t size);
static void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                        char *longmsg);
static void destroy();
static void signal_handler(int sig);

int main(int argc, char *argv[]) {
  int i, listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  atexit(destroy);

  Signal(SIGINT, signal_handler);
  Signal(SIGTERM, signal_handler);
  Signal(SIGQUIT, signal_handler);

  cache_init(&cache, MAX_CACHE_SIZE);

  listenfd = Open_listenfd(argv[1]);

  sbuf_init(&sbuf, SBUFSIZE);
  for (i = 0; i < NTHREADS; i++)
    Pthread_create(&tid, NULL, thread, NULL);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    sbuf_insert(&sbuf, connfd);
  }
  return 0;
}

static void *thread(void *vargp) {
  Pthread_detach(pthread_self());
  while (1) {
    int connfd = sbuf_remove(&sbuf);
    doit(connfd);
    Close(connfd);
  }
  return NULL;
}

static void doit(int fd) {
  char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
  char host[MAXLINE], abs_path[MAXLINE], port[MAX_PORT_LEN];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("%s", buf);

  if (sscanf(buf, "%s %s %s", method, url, version) < 3) {
    clienterror(fd, method, "400", "Bad Request",
                "Request line missing method, URL, or version");
    return;
  }

  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not Implemented",
                "Proxy does not implement this method");
    return;
  }

  parse_url(url, host, port, abs_path);

  printf("method: %s, url: %s, version: %s\n", method, url, version);
  printf("parsed -> host: %s, port: %s, path: %s\n", host, port, abs_path);

  deal_request(fd, host, port, abs_path, url);
}

static void parse_url(char *url, char *host, char *port, char *path) {
  const char *p = url;
  char *host_ptr = host;
  char *port_ptr = port;
  char *path_ptr = path;

  // Skip "http://"
  if (strncmp(p, "http://", 7) == 0) {
    p += 7;
  }

  // Parse host (up to ':' or '/' or end)
  while (*p && *p != ':' && *p != '/') {
    *host_ptr++ = tolower(*p);
    p++;
  }
  *host_ptr = '\0';

  // Parse port
  if (*p == ':') {
    p++;
    while (*p && *p != '/') {
      *port_ptr++ = *p++;
    }
    *port_ptr = '\0';
  } else {
    strcpy(port, "80"); // Default port
  }

  if (*p == '/') {
    while (*p) {
      *path_ptr++ = *p++;
    }
    *path_ptr = '\0';
  } else {
    strcpy(path, "/"); // Default path
  }
}

static void deal_request(int clientfd, char *host, char *port, char *path,
                         char *url) {
  size_t n;
  int serverfd;
  char buf[MAXLINE];
  rio_t rio;
  buffer_t buffer;
  object_t *obj;

  // Search for cached object
  if ((obj = cache_access(&cache, url))) {
    printf("Cache hit, forwarding to client\n");
    Rio_writen(clientfd, obj->data, obj->size);
    return;
  }

  buffer = (buffer_t){0, (char *)Malloc(MAX_OBJECT_SIZE), 1};

  serverfd = Open_clientfd(host, port);
  if (serverfd < 0) {
    clienterror(clientfd, "SERVER_CONNECTION", "503", "Service Unavailable",
                "Could not connect to the server");
    return;
  }

  Rio_readinitb(&rio, serverfd);

  snprintf(buf, MAXLINE, "GET %s HTTP/1.0\r\n", path);
  Rio_writen(serverfd, buf, strlen(buf));

  snprintf(buf, MAXLINE, "Host: %s\r\n", host);
  Rio_writen(serverfd, buf, strlen(buf));

  snprintf(buf, MAXLINE, "%s", user_agent_hdr);
  Rio_writen(serverfd, buf, strlen(buf));

  snprintf(buf, MAXLINE, "Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  snprintf(buf, MAXLINE, "Proxy-Connection: close\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  snprintf(buf, MAXLINE, "\r\n");
  Rio_writen(serverfd, buf, strlen(buf));

  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
    printf("Proxy received %d bytes, forwarding to client\n", (int)n);
    Rio_writen(clientfd, buf, n);

    if (buffer.cacheable) {
      if (buffer.size + n < MAX_OBJECT_SIZE) {
        memcpy(buffer.data + buffer.size, buf, n);
        buffer.size += n;
      } else {
        buffer.cacheable = 0;
        Free(buffer.data);
        buffer.data = NULL;
      }
    }
  }

  if (buffer.cacheable) {
    add_to_cache(url, buffer.data, buffer.size);
  }

  Close(serverfd);
}

static void add_to_cache(char *url, char *data, size_t size) {
  object_t *object = (object_t *)Malloc(sizeof(object_t));
  object->size = size;
  object->url = (char *)Malloc(strlen(url) + 1);
  object->data = data;
  object->next = NULL;
  object->prev = NULL;
  memcpy(object->url, url, strlen(url) + 1);

  cache_insert(&cache, object);
}

static void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                        char *longmsg) {
  char buf[MAXLINE];

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));

  sprintf(buf, "<html><title>Proxy Error</title>");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<body bgcolor="
               "ffffff"
               ">\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<hr><em>The Proxy Server</em>\r\n");
  Rio_writen(fd, buf, strlen(buf));
}

static void destroy() {
  cache_deinit(&cache);
  sbuf_deinit(&sbuf);
}

static void signal_handler(int sig) {
  printf("\nReceived signal %d, initiating shutdown...\n", sig);
  exit(0);
}