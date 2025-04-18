#include <assert.h>
#include <bare.h>
#include <uv.h>

typedef struct {
  uv_tcp_t handle;
  uv_connect_t connect;
  uv_buf_t read;

  js_env_t *env;
  js_ref_t *ctx;
  js_ref_t *on_read;
  js_ref_t *on_close;
  js_deferred_teardown_t *teardown;

  int port;
  char *ip;
  char *request;
} tcpcat_t;

static void call_onread_cb(tcpcat_t* tcpcat, ssize_t nread) {
  int err;
  js_env_t *env = tcpcat->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *ctx;
  err = js_get_reference_value(env, tcpcat->ctx, &ctx);
  assert(err == 0);

  js_value_t *on_read;
  err = js_get_reference_value(env, tcpcat->on_read, &on_read);
  assert(err == 0);

  js_value_t *params[2];
  if (nread < 0) {
    js_value_t *code;
    err = js_create_string_utf8(env, (utf8_t *) uv_err_name(nread), -1, &code);
    assert(err == 0);

    js_value_t *message;
    err = js_create_string_utf8(env, (utf8_t *) uv_strerror(nread), -1, &message);
    assert(err == 0);

    err = js_create_error(env, code, message, &params[0]);
    assert(err == 0);

    err = js_create_int32(env, -1, &params[1]);
    assert(err == 0);
  } else {
    err = js_get_null(env, &params[0]);
    assert(err == 0);

    err = js_create_int32(env, nread, &params[1]);
    assert(err == 0);
  }

  js_call_function(env, ctx, on_read, 2, params, NULL);
}


static void call_onclose_cb(tcpcat_t* tcpcat) {
  int err;
  js_env_t *env = tcpcat->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *ctx;
  err = js_get_reference_value(env, tcpcat->ctx, &ctx);
  assert(err == 0);

  js_value_t *on_close;
  err = js_get_reference_value(env, tcpcat->on_close, &on_close);
  assert(err == 0);

  js_value_t *params[1];
  err = js_get_null(env, &params[0]);
  assert(err == 0);

  js_call_function(env, ctx, on_close, 1, params, NULL);
}

static void tcpcat__on_close(uv_handle_t* handle) {
  int err;
  tcpcat_t *tcpcat = (tcpcat_t *) handle;

  js_env_t *env = tcpcat->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *ctx;
  err = js_get_reference_value(env, tcpcat->ctx, &ctx);
  assert(err == 0);

  call_onclose_cb(tcpcat);

  err = js_delete_reference(env, tcpcat->on_read);
  assert(err == 0);

  err = js_delete_reference(env, tcpcat->on_close);
  assert(err == 0);

  err = js_delete_reference(env, tcpcat->ctx);
  assert(err == 0);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  js_deferred_teardown_t *teardown = tcpcat->teardown;
  err = js_finish_deferred_teardown_callback(teardown);
  assert(err == 0);

  free(tcpcat->ip);
  free(tcpcat->request);
}

static void tcpcat__on_alloc(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  tcpcat_t *tcpcat = (tcpcat_t *) handle;

  *buf = tcpcat->read;
}

static void tcpcat__on_teardown(js_deferred_teardown_t *handle, void *data) {
  tcpcat_t *tcpcat = (tcpcat_t *) data;

  uv_close((uv_handle_t*) &tcpcat->handle, tcpcat__on_close);
}

static void tcpcat__on_write(uv_write_t* req, int status) {
  tcpcat_t *tcpcat = (tcpcat_t *) req->handle;

  if (status < 0) {
    call_onread_cb(tcpcat, status);
    return;
  }

  free(req);
}

static void tcpcat__on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  tcpcat_t *tcpcat = (tcpcat_t *) stream;

  if (nread == UV_EOF) {
    uv_close((uv_handle_t*) stream, tcpcat__on_close);
    return;
  }

  if(nread >= 0) {
    call_onread_cb(tcpcat, nread);
  }
}

static void write2(uv_stream_t* stream, char *data, int len) {
  uv_buf_t buffer[] = {
    {.base = data, .len = len}
  };

  uv_write_t *req = malloc(sizeof(uv_write_t));
  uv_write(req, stream, buffer, 1, tcpcat__on_write);
}

static void tcpcat__on_connect(uv_connect_t *req, int status) {
  tcpcat_t *tcpcat = (tcpcat_t *) req->handle;

  if (status < 0) {
    call_onread_cb(tcpcat, status);
    return;
  }

  write2(req->handle, tcpcat->request, strlen(tcpcat->request));
  uv_read_start(req->handle, tcpcat__on_alloc, tcpcat__on_read);
}

static js_value_t* tcpcat_cat(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 7;
  js_value_t *argv[argc];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0 || argc == 7);

  tcpcat_t *tcpcat;
  js_value_t *handle;
  err = js_create_arraybuffer(env, sizeof(tcpcat_t), (void **) &tcpcat, &handle);
  assert(err == 0);

  err = js_add_deferred_teardown_callback(env, tcpcat__on_teardown, (void *) tcpcat, &tcpcat->teardown);
  assert(err == 0);

  err = js_create_reference(env, argv[0], 1, &tcpcat->ctx);
  assert(err == 0);

  err = js_get_typedarray_info(env, argv[1], NULL, (void **) &tcpcat->read.base, (size_t *) &tcpcat->read.len, NULL, NULL);
  assert(err == 0);

  size_t ip_len;
  err = js_get_value_string_utf8(env, argv[2], NULL, 0, &ip_len);
  assert(err == 0);

  utf8_t *ip = malloc(ip_len);
  err = js_get_value_string_utf8(env, argv[2], ip, ip_len, NULL);
  assert(err == 0);

  uint32_t port;
  err = js_get_value_uint32(env, argv[3], &port);
  assert(err == 0);

  size_t request_len;
  err = js_get_value_string_utf8(env, argv[4], NULL, 0, &request_len);
  assert(err == 0);

  utf8_t *request = malloc(request_len);
  err = js_get_value_string_utf8(env, argv[4], request, request_len, NULL);
  assert(err == 0);

  err = js_create_reference(env, argv[5], 1, &tcpcat->on_read);
  assert(err == 0);

  err = js_create_reference(env, argv[6], 1, &tcpcat->on_close);
  assert(err == 0);

  uv_loop_t *loop;
  err = js_get_env_loop(env, &loop);
  assert(err == 0);
  err = uv_tcp_init(loop, &tcpcat->handle);
  if (err < 0) {
    free(ip);
    free(request);

    js_throw_error(env, uv_err_name(err), uv_strerror(err));
    return NULL;
  }

  tcpcat->env = env;
  tcpcat->port = port;
  tcpcat->ip = strdup((char *)ip);
  tcpcat->request = strdup((char *)request);

  struct sockaddr_storage addr;
  err = uv_ip4_addr((char *) tcpcat->ip, tcpcat->port, (struct sockaddr_in *) &addr);
  if (err < 0) {
    free(ip);
    free(request);

    js_throw_error(env, uv_err_name(err), uv_strerror(err));
    return NULL;
  }

  uv_connect_t *req = &tcpcat->connect;
  req->data = tcpcat;
  err = uv_tcp_connect(&tcpcat->connect, &tcpcat->handle, (struct sockaddr *) &addr, tcpcat__on_connect);

  free(ip);
  free(request);

  return handle;
}

static js_value_t * addon_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("cat", tcpcat_cat)
#undef V

  return exports;
}

BARE_MODULE(bare_addon, addon_exports)
