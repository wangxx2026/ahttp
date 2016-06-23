/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_ahttp.h"

#include "event2/event-config.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "event2/event.h"
#include "event2/http.h"
#include "event2/http_struct.h"

#define URL_MAX 4096

/* If you declare any globals in php_ahttp.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(ahttp)
*/

/* True global resources - no need for thread safety here */
static int le_ahttp;

zend_class_entry *ahttp_entry;


struct readcb_arg
{
	int idx;
	zval *this;
	struct evbuffer *evbuf;
	int len;
};

int php_le_ahttp(void)
{
	return le_ahttp;
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("ahttp.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_ahttp_globals, ahttp_globals)
    STD_PHP_INI_ENTRY("ahttp.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_ahttp_globals, ahttp_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_ahttp_compiled(string arg)
   Return a string to confirm that the module is compiled in */

/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/

static void
http_request_done(struct evhttp_request *req, void *ctx)
{
	zend_class_entry *ce;
	zval *result_arr, result, *z_rsrc, *error_arr, *response_arr, *response_res, *tmp_reponse;
	struct readcb_arg *arg = ctx;
	char *strg;
	int http_code;


	ce = Z_OBJCE_P(arg->this);
	result_arr = zend_read_property(ce, arg->this, "result_arr", sizeof("result_arr") - 1, 0 TSRMLS_CC);
	response_arr = zend_read_property(ce, arg->this, "response_arr", sizeof("response_arr") -1, 0 TSRMLS_CC);

	MAKE_STD_ZVAL(tmp_reponse);
	array_init(tmp_reponse);
	if(req)
	{
		int body_size = req->body_size;

		struct evbuffer *input = evhttp_request_get_input_buffer(req);
		size_t len = evbuffer_get_length(input), s_len;

		s_len = spprintf(&strg, body_size, "%s", evbuffer_pullup(input, len));

		http_code = evhttp_request_get_response_code(req);
		add_assoc_string(tmp_reponse, "message", "", 1);

	}
	else
	{

		http_code = 503;
		add_assoc_string(tmp_reponse, "message", "Service Unavailable", 1);
		spprintf(&strg, 0, "%s", "");
	}
	add_assoc_long(tmp_reponse, "http_code", http_code);
	add_index_string(result_arr, arg->idx, strg, 1);
	add_index_zval(response_arr, arg->idx, tmp_reponse);
	efree(strg);
}



/* {{{ php_ahttp_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_ahttp_init_globals(zend_ahttp_globals *ahttp_globals)
{
	ahttp_globals->global_value = 0;
	ahttp_globals->global_string = NULL;
}
*/
/* }}} */

PHP_METHOD(ahttp, __construct)
{
	zval *url_arr, *result_arr, *response_arr, error_arr, header_arr, *rsrc_rst;
	struct event_base *base;
	int rsrc;
	int le_ahttp = php_le_ahttp();

	struct event_config *evconfig = event_config_new();
	event_config_avoid_method(evconfig ,"select");
	event_config_avoid_method(evconfig ,"poll");

	base = event_base_new_with_config(evconfig);
	event_config_free(evconfig);

	MAKE_STD_ZVAL(rsrc_rst);
	MAKE_STD_ZVAL(url_arr);
	MAKE_STD_ZVAL(result_arr);
	MAKE_STD_ZVAL(response_arr);

	rsrc = zend_register_resource(rsrc_rst, base, le_ahttp);
	array_init(url_arr);
	array_init(result_arr);
	array_init(response_arr);

	add_property_zval(getThis(), "base", rsrc_rst);
	add_property_long(getThis(), "time_out", 2000);
	add_property_zval(getThis(), "url_arr", url_arr);
	add_property_zval(getThis(), "result_arr", result_arr);
	add_property_zval(getThis(), "response_arr", response_arr);

	zval_ptr_dtor(&url_arr);
	zval_ptr_dtor(&result_arr);
	zval_ptr_dtor(&response_arr);
	zval_ptr_dtor(&rsrc_rst);
}

PHP_METHOD(ahttp, get)
{
	char *url = NULL;
	size_t url_len;

	zval *z_arr, *url_arr, *opt_arr = NULL;
	zend_class_entry *ce;
	HashTable *url_ht;

	ce = Z_OBJCE_P(getThis());
	z_arr = zend_read_property(ce, getThis(), "url_arr", sizeof("url_arr") - 1, 0 TSRMLS_CC);
	url_ht = Z_ARRVAL_P(z_arr);

	if(url_ht->nNumOfElements == 50)
	{
		php_error_docref(NULL, E_ERROR, "too many request, should  be less than 50");
	}

	if(zend_parse_parameters(ZEND_NUM_ARGS(), "s|a", &url, &url_len, &opt_arr) == FAILURE)
	{
		return;
	}

	{
		char *method = "get";
		struct evhttp_uri *http_uri;
		const char *scheme, *host, *path, *query;
		int port;

		http_uri = evhttp_uri_parse(url);
		if (http_uri == NULL) {
			php_error_docref(NULL, E_ERROR, "malformed url");
		}
		scheme = evhttp_uri_get_scheme(http_uri);
		host = evhttp_uri_get_host(http_uri);
		port = evhttp_uri_get_port(http_uri);
		path = evhttp_uri_get_path(http_uri);
		query = evhttp_uri_get_query(http_uri);

		if (scheme == NULL || (strcasecmp(scheme, "https") != 0 && strcasecmp(scheme, "http") != 0)) {
			php_error_docref(NULL, E_ERROR, "url must be http or https");
		}

		if (host == NULL) {
			php_error_docref(NULL, E_ERROR, "url must have a host");
		}

		MAKE_STD_ZVAL(url_arr);
		array_init(url_arr);
		add_assoc_string(url_arr, "url", url, 1);
		add_assoc_string(url_arr, "method",  method, 1);

		// 处理参数选项
		if(opt_arr)
		{
			zval **header, *tmpcopy;
			char *header_key = "header";
			if(zend_hash_exists(Z_ARRVAL_P(opt_arr), header_key, strlen(header_key) + 1))
			{
				zend_hash_find(Z_ARRVAL_P(opt_arr), header_key, strlen(header_key) + 1, (void **)&header);

				MAKE_STD_ZVAL(tmpcopy);
				MAKE_COPY_ZVAL(header, tmpcopy);
				convert_to_array(tmpcopy);
				add_assoc_zval(url_arr, "header", tmpcopy);
			}
		}
		add_next_index_zval(z_arr, url_arr);
	}
}


PHP_METHOD(ahttp, post)
{
	char *url = NULL;
	size_t url_len;

	zval *z_arr, *url_arr, *opt_arr = NULL;
	zend_class_entry *ce;
	HashTable *url_ht;

	ce = Z_OBJCE_P(getThis());
	z_arr = zend_read_property(ce, getThis(), "url_arr", sizeof("url_arr") - 1, 0 TSRMLS_CC);
	url_ht = Z_ARRVAL_P(z_arr);

	if(url_ht->nNumOfElements == 50)
	{
		php_error_docref(NULL, E_ERROR, "too many request, should  be less than 50");
	}

	if(zend_parse_parameters(ZEND_NUM_ARGS(), "s|a", &url, &url_len, &opt_arr) == FAILURE)
	{
		return;
	}

	{
		char *method = "post";
		struct evhttp_uri *http_uri;
		const char *scheme, *host, *path, *query;
		int port;

		http_uri = evhttp_uri_parse(url);
		if (http_uri == NULL) {
			php_error_docref(NULL, E_ERROR, "malformed url");
		}
		scheme = evhttp_uri_get_scheme(http_uri);
		host = evhttp_uri_get_host(http_uri);
		port = evhttp_uri_get_port(http_uri);
		path = evhttp_uri_get_path(http_uri);
		query = evhttp_uri_get_query(http_uri);

		if (scheme == NULL || (strcasecmp(scheme, "https") != 0 && strcasecmp(scheme, "http") != 0)) {
			php_error_docref(NULL, E_ERROR, "url must be http or https");
		}

		if (host == NULL) {
			php_error_docref(NULL, E_ERROR, "url must have a host");
		}

		MAKE_STD_ZVAL(url_arr);
		array_init(url_arr);
		add_assoc_string(url_arr, "url", url, 1);
		add_assoc_string(url_arr, "method",  method, 1);

		// 处理参数选项
		if(opt_arr)
		{
			zval **header, **post_data, *tmpcopy, p_tmpcopy;
			char *header_key = "header";
			char *data_key = "data";

			if(zend_hash_exists(Z_ARRVAL_P(opt_arr), header_key, strlen(header_key) + 1))
			{
				zend_hash_find(Z_ARRVAL_P(opt_arr), header_key, strlen(header_key) + 1, (void **)&header);

				MAKE_STD_ZVAL(tmpcopy);
				MAKE_COPY_ZVAL(header, tmpcopy);
				convert_to_array(tmpcopy);
				add_assoc_zval(url_arr, "header", tmpcopy);
			}

			if(zend_hash_exists(Z_ARRVAL_P(opt_arr), data_key, strlen(data_key) + 1))
			{
				zend_hash_find(Z_ARRVAL_P(opt_arr), data_key, strlen(data_key) + 1, (void **)&post_data);

				p_tmpcopy = **post_data;
				INIT_PZVAL(&p_tmpcopy);
				convert_to_string(&p_tmpcopy);

				add_assoc_string(url_arr, "data", Z_STRVAL(p_tmpcopy), 1);
			}
		}

		add_next_index_zval(z_arr, url_arr);
	}
}

PHP_METHOD(ahttp, wait_reply)
{
	struct event_base *base;
	zval *z_base, *url_arr, *time_out;
	zval *base_rsrc;
	HashTable *url_ht;
	int sec, msec;

	zend_class_entry *ce = Z_OBJCE_P(getThis());
	z_base = zend_read_property(ce, getThis(), "base", sizeof("base") - 1, 0 TSRMLS_CC);
	ZEND_FETCH_RESOURCE(base, struct event_base *, &z_base, -1,  AHTTP_RES_NAME, le_ahttp);

	url_arr = zend_read_property(ce, getThis(), "url_arr", sizeof("url_arr") - 1, 0 TSRMLS_CC);
	url_ht = Z_ARRVAL_P(url_arr);

	time_out = zend_read_property(ce, getThis(), "time_out", sizeof("time_out") - 1, 0 TSRMLS_CC);

	int arr_size = url_ht->nNumOfElements;
	struct evhttp_uri *http_uri[arr_size], *location[arr_size];
	const char *scheme[arr_size], *host[arr_size], *path[arr_size],
	*query[arr_size];
	int port[arr_size];
	char *port_s[arr_size];
	struct readcb_arg cb_arg[arr_size];
	struct evhttp_connection *evcon[arr_size];
	struct evhttp_request *req[arr_size];
	char buffer[arr_size][URL_MAX];
	char uri[arr_size][256];
	struct bufferevent *bev[arr_size];
	struct evkeyvalq *output_headers[arr_size];
	struct evbuffer *output_buffer[arr_size];

	char *url_key = "url";
	char *method_key = "method";
	char *header_key = "header";
	char *data_key = "data";

	for(zend_hash_internal_pointer_reset(url_ht); zend_hash_has_more_elements(url_ht) == SUCCESS; zend_hash_move_forward(url_ht))
	{
		char *key;
		uint keylen;
		zend_ulong idx;
		int type;
		zval **ppzval, tmpcopy, **purl, **method, tmp_url, tmp_method;
		HashTable *url_arr;

		type = zend_hash_get_current_key_ex(url_ht, &key, &keylen, &idx, 0, NULL);
		zend_hash_get_current_data(url_ht, (void**)&ppzval);

		tmpcopy = **ppzval;
		INIT_PZVAL(&tmpcopy);
		convert_to_array(&tmpcopy);

		zend_hash_find(Z_ARRVAL(tmpcopy), url_key, strlen(url_key) + 1, (void **)&purl);
		zend_hash_find(Z_ARRVAL(tmpcopy), method_key, strlen(method_key) + 1, (void **)&method);

		tmp_url = **purl;
		INIT_PZVAL(&tmp_url);
		convert_to_string(&tmp_url);

		tmp_method = **method;
		INIT_PZVAL(&tmp_method);
		convert_to_string(&tmp_method);

		char *str_method = Z_STRVAL(tmp_method);

		http_uri[idx] = evhttp_uri_parse(Z_STRVAL(tmp_url));
		scheme[idx] = evhttp_uri_get_scheme(http_uri[idx]);
		host[idx] = evhttp_uri_get_host(http_uri[idx]);
		port[idx] = evhttp_uri_get_port(http_uri[idx]);
		path[idx] = evhttp_uri_get_path(http_uri[idx]);
		query[idx] = evhttp_uri_get_query(http_uri[idx]);

		if (port[idx] == -1) {
			port[idx] = (strcasecmp(scheme[idx], "http") == 0) ? 80 : 443;
		}

		if (strlen(path[idx]) == 0) {
			path[idx] = "/";
			evhttp_uri_set_path(http_uri[idx], path[idx]);
		}

		location[idx] = http_uri[idx];
		evhttp_uri_set_scheme(location[idx], NULL);
		evhttp_uri_set_userinfo(location[idx], 0);
		evhttp_uri_set_host(location[idx], NULL);
		evhttp_uri_set_port(location[idx], -1);

		evcon[idx] = evhttp_connection_base_new(base, NULL, host[idx], port[idx]);

		cb_arg[idx] = (struct readcb_arg){
			.idx = idx,
			.this = getThis(),
			.evbuf = evbuffer_new(),
			.len = 0,
		};

		req[idx] = evhttp_request_new(http_request_done, &cb_arg[idx]);
		output_headers[idx] = evhttp_request_get_output_headers(req[idx]);
		evhttp_add_header(output_headers[idx], "Host", host[idx]);

		// 自定义头信息
		{
			if(zend_hash_exists(Z_ARRVAL(tmpcopy), header_key, strlen(header_key) + 1))
			{
				zval **hv_zval, h_tmpcopy, hk_con, *oh_tmpcopy, **header;
				HashTable *header_ht;
				int hk_type;
				char *h_key;

				zend_hash_find(Z_ARRVAL_PP(ppzval), header_key, strlen(header_key) + 1, (void **)&header);


				MAKE_STD_ZVAL(oh_tmpcopy);
				MAKE_COPY_ZVAL(header, oh_tmpcopy);
				convert_to_array(oh_tmpcopy);
				// header 转成数组类型
				header_ht = Z_ARRVAL_P(oh_tmpcopy);

				for(zend_hash_internal_pointer_reset(header_ht); zend_hash_has_more_elements(header_ht) == SUCCESS; zend_hash_move_forward(header_ht))
				{
					zend_ulong h_idx;
					hk_type = zend_hash_get_current_key(header_ht, &h_key, &h_idx, 0);
					zend_hash_get_current_data(header_ht, (void **) &hv_zval);
					if(hk_type ==  HASH_KEY_IS_STRING)
					{
						h_tmpcopy = **hv_zval;
						convert_to_string(&h_tmpcopy);
						//ZVAL_NEW_STR(&hk_con, h_key);
						evhttp_add_header(output_headers[idx], h_key, Z_STRVAL(h_tmpcopy));
					}
					zval_dtor(&h_tmpcopy);
				}
				zval_ptr_dtor(&oh_tmpcopy);
			}
		}
		// 自定义头信息end

		// 处理post请求
		if(strcasecmp(str_method, "post")  == 0)
		{
			if(zend_hash_exists(Z_ARRVAL_PP(ppzval), data_key, strlen(data_key) + 1))
			{
				char buf[1024];
				zval **post_data, p_tmpcoy;
				struct evbuffer *output_buffer = evhttp_request_get_output_buffer(req[idx]);
				zend_hash_find(Z_ARRVAL_PP(ppzval), data_key, strlen(data_key) + 1, (void **)&post_data);

				p_tmpcoy = **post_data;
				INIT_PZVAL(&p_tmpcoy);
				convert_to_string(&p_tmpcoy);

				evbuffer_add(output_buffer, Z_STRVAL(p_tmpcoy), Z_STRLEN(p_tmpcoy));
				int len = Z_STRLEN(p_tmpcoy);
				evutil_snprintf(buf, sizeof(buf)-1, "%lu", (unsigned long)len);
				evhttp_add_header(output_headers[idx], "Content-Length", buf);
			}
		}

		// 处理post请求 end
		evhttp_add_header(output_headers[idx], "Connection", "close");

		//evhttp_connection_set_timeout
		evhttp_connection_set_timeout(evcon[idx], Z_LVAL_P(time_out));
		evhttp_connection_set_retries(evcon[idx], 2);
		evhttp_make_request(evcon[idx], req[idx], (strcasecmp(str_method, "get")  == 0) ? EVHTTP_REQ_GET : EVHTTP_REQ_POST, evhttp_uri_join(location[idx], buffer[idx], URL_MAX));
	}
	event_base_dispatch(base);
}

PHP_METHOD(ahttp, result)
{
	zval *result_arr;
	zend_class_entry *ce;

	ce = Z_OBJCE_P(getThis());
	result_arr = zend_read_property(ce, getThis(), "result_arr", sizeof("result_arr") - 1, 0 TSRMLS_CC);
	RETURN_ZVAL(result_arr, 1 , 0);
	return;
}


PHP_METHOD(ahttp, set_time_out)
{

	long msec;
	if(zend_parse_parameters(ZEND_NUM_ARGS(), "l", &msec) == FAILURE)
	{
		return;
	}
	zend_class_entry *ce;

	ce = Z_OBJCE_P(getThis());
	zend_update_property_long(ce, getThis(), "time_out", sizeof("time_out") - 1, msec);
}
const zend_function_entry ahttp_ce_functions[] = {
		PHP_ME(ahttp, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
		PHP_ME(ahttp, get, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(ahttp, post, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(ahttp, result, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(ahttp, set_time_out, NULL, ZEND_ACC_PUBLIC)
		PHP_ME(ahttp, wait_reply, NULL, ZEND_ACC_PUBLIC)
};

static void _php_event_base_dtor(zend_rsrc_list_entry *rsrc) /* {{{ */
{
	struct event_base *base = (struct event_base*)rsrc->ptr;
	event_base_free(base);
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(ahttp)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/

	le_ahttp = zend_register_list_destructors_ex(_php_event_base_dtor, NULL, AHTTP_RES_NAME, module_number);

	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "ahttp", ahttp_ce_functions);
	ahttp_entry = zend_register_internal_class(&ce TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(ahttp)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(ahttp)
{
#if defined(COMPILE_DL_AHTTP) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(ahttp)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(ahttp)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "ahttp support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ ahttp_functions[]
 *
 * Every user visible function must have an entry in ahttp_functions[].
 */
PHP_FUNCTION(ahttp_version){
	RETVAL_STRING(PHP_AHTTP_VERSION, 0);
}
const zend_function_entry ahttp_functions[] = {
	PHP_FE(ahttp_version,	NULL)
	PHP_FE_END
};
/* }}} */

/* {{{ ahttp_module_entry
 */
zend_module_entry ahttp_module_entry = {
	STANDARD_MODULE_HEADER,
	"ahttp",
	ahttp_functions,//ahttp_functions,
	PHP_MINIT(ahttp),
	PHP_MSHUTDOWN(ahttp),
	PHP_RINIT(ahttp),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(ahttp),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(ahttp),
	PHP_AHTTP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_AHTTP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(ahttp)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
