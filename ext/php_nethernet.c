/*
 * MIT License
 *
 * Copyright (c) 2026 vp817
 * Copyright (c) 2026 Laith98Dev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(_MSC_VER) && !defined(__cplusplus)
typedef double max_align_t;
#endif

#include <php.h>
#include <ext/standard/info.h>
#include <ext/json/php_json.h>
#include <zend_exceptions.h>
#include <zend_smart_str.h>
#include <ext/spl/spl_exceptions.h>

#ifdef NN_PHP_BUILD_ID
#ifdef ZEND_MODULE_BUILD_ID
#undef ZEND_MODULE_BUILD_ID
#endif
#define ZEND_MODULE_BUILD_ID NN_PHP_BUILD_ID
#endif

#include <nethernet/nn_peer.h>

#define PHP_NETHERNET_VERSION "0.1.0"

typedef struct
{
	NetherNet nn;
	bool initialized;
	bool started;
	zend_object std;
} php_nethernet_server;

static zend_class_entry *nethernet_server_ce = NULL;
static zend_class_entry *nethernet_exception_ce = NULL;
static zend_object_handlers nethernet_server_handlers;

static zend_class_entry *nethernet_client_ce = NULL;

static inline php_nethernet_server *server_from_obj(zend_object *obj)
{
	return (php_nethernet_server *)((char *)obj - XtOffsetOf(php_nethernet_server, std));
}

#define Z_NETHERNET_SERVER_P(zv) server_from_obj(Z_OBJ_P(zv))

static zend_object *nethernet_server_create(zend_class_entry *ce)
{
	php_nethernet_server *intern = zend_object_alloc(sizeof(php_nethernet_server), ce);
	memset(&intern->nn, 0, sizeof(intern->nn));
	intern->initialized = false;
	intern->started = false;
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &nethernet_server_handlers;
	return &intern->std;
}

static void server_shutdown(php_nethernet_server *intern)
{
	if (intern->initialized) {
		nn_destroy(&intern->nn);
		intern->initialized = false;
		intern->started = false;
	}
}

static void free_options(php_nethernet_server *intern)
{
	if (intern->nn.operator_key_path) {
		efree((void *)intern->nn.operator_key_path);
		intern->nn.operator_key_path = NULL;
	}
	if (intern->nn.operator_domain) {
		efree((void *)intern->nn.operator_domain);
		intern->nn.operator_domain = NULL;
	}
	const char **network[] = { &intern->nn.advertise_addresses, &intern->nn.trusted_proxies, &intern->nn.real_ip_header };
	for (size_t i = 0; i < sizeof(network) / sizeof(network[0]); ++i) {
		if (*network[i]) {
			efree((void *)*network[i]);
			*network[i] = NULL;
		}
	}
	if (intern->nn.tls_cert_path) {
		efree((void *)intern->nn.tls_cert_path);
		intern->nn.tls_cert_path = NULL;
	}
	if (intern->nn.tls_key_path) {
		efree((void *)intern->nn.tls_key_path);
		intern->nn.tls_key_path = NULL;
	}
}

static void nethernet_server_free(zend_object *obj)
{
	php_nethernet_server *intern = server_from_obj(obj);
	server_shutdown(intern);
	free_options(intern);
	zend_object_std_dtor(obj);
}

static const char *result_str(NetherNetResult r)
{
	switch (r) {
		case NN_SUCCESS: return "success";
		case NN_NN_INVALID: return "invalid instance";
		case NN_TISS_FAILED: return "unable to fetch the minecraft signing keys (trusted issuer)";
		case NN_ALREADY_INITIALIZED: return "already initialized";
		case NN_HTTP_THREAD_CREATION_FAIL: return "unable to create the http thread";
		case NN_HTTP_THREAD_DETACH_FAIL: return "unable to detach the http thread";
		case NN_NOT_INITIALIZED: return "not initialized";
		case NN_OPERATOR_KEY_FAIL: return "unable to load or create the operator key";
		case NN_LISTEN_FAIL: return "unable to listen on the signaling url";
		case NN_INVALID_ARGS: return "invalid arguments";
		case NN_CONNECTION_NOT_FOUND: return "connection not found";
		case NN_CONNECTION_NOT_READY: return "connection not ready";
		case NN_MESSAGE_TOO_LARGE: return "message too large";
		case NN_SEND_FAIL: return "send failed";
		case NN_HTTP_FAIL: return "signaling request failed";
		case NN_CLIENT_KEY_FAIL: return "invalid client key (P-384 PEM matching the token's cpk)";
		case NN_SERVER_REJECTED: return "the server refused the offer";
		case NN_SERVER_IDENTITY_INVALID: return "the server identity is missing or invalid";
		case NN_SERVER_IDENTITY_MISMATCH: return "the server key does not match the expected fingerprint";
		case NN_WEBRTC_FAIL: return "webrtc connection failed";
		case NN_TIMEOUT: return "timed out";
		case NN_TLS_FAIL: return "unable to read the tls certificate or key";
		case NN_CA_STORE_FAIL: return "no trusted CA certificates found for https (set SSL_CERT_FILE to a PEM bundle)";
		default: return "unknown error";
	}
}

static char *option_string(HashTable *options, const char *key)
{
	zval *v = zend_hash_str_find(options, key, strlen(key));
	if (!v || Z_TYPE_P(v) == IS_NULL) {
		return NULL;
	}
	if (Z_TYPE_P(v) != IS_STRING) {
		zend_throw_exception_ex(nethernet_exception_ce, 0, "Option \"%s\" must be a string", key);
		return NULL;
	}
	if (Z_STRLEN_P(v) != strlen(Z_STRVAL_P(v))) {
		zend_throw_exception_ex(nethernet_exception_ce, 0, "Option \"%s\" must not contain NUL bytes", key);
		return NULL;
	}
	return estrdup(Z_STRVAL_P(v));
}

static bool option_int(HashTable *options, const char *key, int *out)
{
	zval *v = zend_hash_str_find(options, key, strlen(key));
	if (!v || Z_TYPE_P(v) == IS_NULL) {
		return true;
	}
	if (Z_TYPE_P(v) != IS_LONG) {
		zend_throw_exception_ex(nethernet_exception_ce, 0, "Option \"%s\" must be an int", key);
		return false;
	}
	if (Z_LVAL_P(v) < 0 || Z_LVAL_P(v) > 1000000) {
		zend_throw_exception_ex(nethernet_exception_ce, 0, "Option \"%s\" must be between 0 and 1000000", key);
		return false;
	}
	*out = (int)Z_LVAL_P(v);
	return true;
}

static bool option_bool(HashTable *options, const char *key, bool *out)
{
	zval *v = zend_hash_str_find(options, key, strlen(key));
	if (!v || Z_TYPE_P(v) == IS_NULL) {
		return true;
	}
	if (Z_TYPE_P(v) == IS_TRUE) {
		*out = true;
		return true;
	}
	if (Z_TYPE_P(v) == IS_FALSE) {
		*out = false;
		return true;
	}
	zend_throw_exception_ex(nethernet_exception_ce, 0, "Option \"%s\" must be a bool", key);
	return false;
}

PHP_METHOD(NetherNet_Server, __construct)
{
	HashTable *options = NULL;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_HT(options)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (intern->initialized || intern->started) {
		zend_throw_exception(nethernet_exception_ce, "Server is already constructed and running", 0);
		RETURN_THROWS();
	}
	if (!options) {
		return;
	}

	if (intern->nn.operator_key_path) {
		efree((void *)intern->nn.operator_key_path);
		intern->nn.operator_key_path = NULL;
	}
	intern->nn.operator_key_path = option_string(options, "operator_key_path");
	if (EG(exception)) {
		return;
	}
	if (intern->nn.operator_domain) {
		efree((void *)intern->nn.operator_domain);
		intern->nn.operator_domain = NULL;
	}
	intern->nn.operator_domain = option_string(options, "operator_domain");
	if (EG(exception)) {
		return;
	}
	if (intern->nn.tls_cert_path) {
		efree((void *)intern->nn.tls_cert_path);
		intern->nn.tls_cert_path = NULL;
	}
	intern->nn.tls_cert_path = option_string(options, "tls_cert_path");
	if (EG(exception)) {
		return;
	}
	if (intern->nn.tls_key_path) {
		efree((void *)intern->nn.tls_key_path);
		intern->nn.tls_key_path = NULL;
	}
	intern->nn.tls_key_path = option_string(options, "tls_key_path");
	if (EG(exception)) {
		return;
	}
	static const char *network_keys[] = { "advertise_addresses", "trusted_proxies", "real_ip_header" };
	const char **network_fields[] = { &intern->nn.advertise_addresses, &intern->nn.trusted_proxies, &intern->nn.real_ip_header };
	for (size_t i = 0; i < sizeof(network_keys) / sizeof(network_keys[0]); ++i) {
		if (*network_fields[i]) {
			efree((void *)*network_fields[i]);
			*network_fields[i] = NULL;
		}
		*network_fields[i] = option_string(options, network_keys[i]);
		if (EG(exception)) {
			return;
		}
	}
	if (!option_int(options, "max_pending", &intern->nn.max_pending) ||
		!option_int(options, "max_pending_per_ip", &intern->nn.max_pending_per_ip) ||
		!option_int(options, "max_connections_per_ip", &intern->nn.max_connections_per_ip) ||
		!option_int(options, "ice_port", &intern->nn.ice_port) ||
		!option_int(options, "mtu", &intern->nn.mtu)
	) {
		RETURN_THROWS();
	}
	bool operator_cpk_spki = false;
	bool allow_unauthenticated = false;
	if (!option_bool(options, "operator_cpk_spki", &operator_cpk_spki) ||
		!option_bool(options, "allow_unauthenticated", &allow_unauthenticated)
	) {
		RETURN_THROWS();
	}
	intern->nn.operator_cpk_spki = operator_cpk_spki;
	intern->nn.allow_unauthenticated = allow_unauthenticated;
}

static bool set_information(php_nethernet_server *intern, zval *info)
{
	smart_str buf = {0};
	if (php_json_encode(&buf, info, PHP_JSON_UNESCAPED_UNICODE | PHP_JSON_UNESCAPED_SLASHES) == FAILURE) {
		smart_str_free(&buf);
		zend_throw_exception(nethernet_exception_ce, "Unable to encode the server information", 0);
		return false;
	}
	smart_str_0(&buf);
	bool ok = nn_set_server_information(&intern->nn, ZSTR_VAL(buf.s));
	smart_str_free(&buf);
	if (!ok) {
		zend_throw_exception(nethernet_exception_ce, "Unable to set the server information", 0);
	}
	return ok;
}

PHP_METHOD(NetherNet_Server, start)
{
	char *url;
	size_t url_len;
	zval *info;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_PATH(url, url_len)
		Z_PARAM_ARRAY(info)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (intern->started) {
		zend_throw_exception(nethernet_exception_ce, "Server is already started", 0);
		RETURN_THROWS();
	}
	NetherNetResult r = nn_peer_init(&intern->nn);
	if (r != NN_SUCCESS) {
		zend_throw_exception_ex(nethernet_exception_ce, r, "Unable to initialize NetherNet: %s", result_str(r));
		RETURN_THROWS();
	}
	intern->initialized = true;

	if (!set_information(intern, info)) {
		server_shutdown(intern);
		RETURN_THROWS();
	}

	r = nn_startup(&intern->nn, url);
	if (r != NN_SUCCESS) {
		server_shutdown(intern);
		zend_throw_exception_ex(nethernet_exception_ce, r, "Unable to start NetherNet: %s", result_str(r));
		RETURN_THROWS();
	}
	intern->started = true;
}

PHP_METHOD(NetherNet_Server, setInformation)
{
	zval *info;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY(info)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (!intern->started) {
		zend_throw_exception(nethernet_exception_ce, "Server is not started", 0);
		RETURN_THROWS();
	}
	set_information(intern, info);
}

PHP_METHOD(NetherNet_Server, update)
{
	ZEND_PARSE_PARAMETERS_NONE();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (intern->started) {
		nn_update_cycle(&intern->nn);
	}
}

PHP_METHOD(NetherNet_Server, pollEvent)
{
	ZEND_PARSE_PARAMETERS_NONE();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	NetherNetEvent ev;
	if (!intern->started || !nn_poll_event(&intern->nn, &ev)) {
		RETURN_NULL();
	}

	array_init_size(return_value, 9);
	add_assoc_long(return_value, "type", (zend_long)ev.type);
	add_assoc_long(return_value, "id", (zend_long)ev.connection_id);
	add_assoc_string(return_value, "networkId", ev.network_id);
	add_assoc_string(return_value, "xuid", ev.identity.xid);
	add_assoc_string(return_value, "name", ev.identity.xname);
	char address[64] = {};
	zend_long port = 0;
	char *colon = strrchr(ev.remote_address, ':');
	if (colon) {
		port = (zend_long)atoi(colon + 1);
		size_t start = ev.remote_address[0] == '[' ? 1 : 0;
		size_t len = (size_t)(colon - ev.remote_address) - start;
		if (len && ev.remote_address[start + len - 1] == ']') {
			--len;
		}
		if (len < sizeof(address)) {
			memcpy(address, ev.remote_address + start, len);
		}
	}
	add_assoc_string(return_value, "address", address);
	add_assoc_long(return_value, "port", port);
	add_assoc_long(return_value, "channel", (zend_long)ev.channel);
	if (ev.type == NN_EVENT_DATA) {
		add_assoc_stringl(return_value, "data", (const char *)ev.data, ev.len);
	}
	nn_free_event(&ev);
}

PHP_METHOD(NetherNet_Server, send)
{
	zend_long id;
	char *payload;
	size_t payload_len;
	zend_long channel = NN_CHANNEL_RELIABLE;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_LONG(id)
		Z_PARAM_STRING(payload, payload_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(channel)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (!intern->started) {
		RETURN_LONG(NN_NOT_INITIALIZED);
	}
	if (channel != NN_CHANNEL_RELIABLE && channel != NN_CHANNEL_UNRELIABLE) {
		zend_argument_value_error(3, "must be Server::CHANNEL_RELIABLE or Server::CHANNEL_UNRELIABLE");
		RETURN_THROWS();
	}
	RETURN_LONG(nn_send(&intern->nn, (uint64_t)id, payload, payload_len, (NetherNetChannel)channel));
}

PHP_METHOD(NetherNet_Server, disconnect)
{
	zend_long id;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(id)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	RETURN_BOOL(intern->started && nn_disconnect(&intern->nn, (uint64_t)id) == NN_SUCCESS);
}

PHP_METHOD(NetherNet_Server, close)
{
	ZEND_PARSE_PARAMETERS_NONE();

	server_shutdown(Z_NETHERNET_SERVER_P(ZEND_THIS));
}

PHP_METHOD(NetherNet_Server, isRunning)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(Z_NETHERNET_SERVER_P(ZEND_THIS)->started);
}

static php_nethernet_server *started_server(zval *this_ptr)
{
	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(this_ptr);
	if (!intern->started) {
		zend_throw_exception(nethernet_exception_ce, "Server is not started", 0);
		return NULL;
	}
	return intern;
}

static void acl_set_list(INTERNAL_FUNCTION_PARAMETERS, NetherNetAclList list)
{
	HashTable *entries;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(entries)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = started_server(ZEND_THIS);
	if (!intern) {
		RETURN_THROWS();
	}
	uint32_t count = zend_hash_num_elements(entries);
	const char **items = count ? (const char **)safe_emalloc(count, sizeof(char *), 0) : NULL;
	uint32_t n = 0;
	zval *v;
	ZEND_HASH_FOREACH_VAL(entries, v) {
		ZVAL_DEREF(v);
		if (Z_TYPE_P(v) != IS_STRING) {
			if (items) efree(items);
			zend_argument_type_error(1, "must be a list of strings, %s given in it", zend_zval_type_name(v));
			RETURN_THROWS();
		}
		if (Z_STRLEN_P(v) == strlen(Z_STRVAL_P(v))) {
			items[n++] = Z_STRVAL_P(v);
		}
	} ZEND_HASH_FOREACH_END();

	bool ok = nn_acl_set(&intern->nn, list, items, n);
	if (items) efree(items);
	if (!ok) {
		zend_throw_exception(nethernet_exception_ce, "Unable to set the list (out of memory)", 0);
		RETURN_THROWS();
	}
}

PHP_METHOD(NetherNet_Server, setBannedNames)
{
	acl_set_list(INTERNAL_FUNCTION_PARAM_PASSTHRU, NN_ACL_BANNED_NAMES);
}

PHP_METHOD(NetherNet_Server, setBannedIps)
{
	acl_set_list(INTERNAL_FUNCTION_PARAM_PASSTHRU, NN_ACL_BANNED_IPS);
}

PHP_METHOD(NetherNet_Server, setWhitelist)
{
	acl_set_list(INTERNAL_FUNCTION_PARAM_PASSTHRU, NN_ACL_WHITELIST);
}

PHP_METHOD(NetherNet_Server, setWhitelistEnabled)
{
	bool enabled;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_BOOL(enabled)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = started_server(ZEND_THIS);
	if (!intern) {
		RETURN_THROWS();
	}
	nn_acl_set_whitelist_enabled(&intern->nn, enabled);
}

PHP_METHOD(NetherNet_Server, setRejectPolicy)
{
	bool banned, not_whitelisted, when_full;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_BOOL(banned)
		Z_PARAM_BOOL(not_whitelisted)
		Z_PARAM_BOOL(when_full)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = started_server(ZEND_THIS);
	if (!intern) {
		RETURN_THROWS();
	}
	nn_acl_set_policy(&intern->nn, banned, not_whitelisted, when_full);
}

PHP_METHOD(NetherNet_Server, setCapacity)
{
	zend_long players, max_players;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(players)
		Z_PARAM_LONG(max_players)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = started_server(ZEND_THIS);
	if (!intern) {
		RETURN_THROWS();
	}
	players = players < 0 ? 0 : players > INT_MAX ? INT_MAX : players;
	max_players = max_players < 0 ? 0 : max_players > INT_MAX ? INT_MAX : max_players;
	nn_acl_set_capacity(&intern->nn, (int)players, (int)max_players);
}

static bool client_ensure_init(php_nethernet_server *intern)
{
	if (intern->initialized) {
		return true;
	}
	NetherNetResult r = nn_peer_init(&intern->nn);
	if (r != NN_SUCCESS) {
		zend_throw_exception_ex(nethernet_exception_ce, r, "Unable to initialize NetherNet: %s", result_str(r));
		return false;
	}
	intern->initialized = true;
	intern->started = true;
	return true;
}

PHP_METHOD(NetherNet_Client, query)
{
	char *url;
	size_t url_len;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_PATH(url, url_len)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (!client_ensure_init(intern)) {
		RETURN_THROWS();
	}
	char *body = nn_query(&intern->nn, url);
	if (!body) {
		RETURN_NULL();
	}
	zval decoded;
	int err = php_json_decode_ex(&decoded, body, strlen(body), PHP_JSON_OBJECT_AS_ARRAY, 32);
	free(body);
	if (err == FAILURE || Z_TYPE(decoded) != IS_ARRAY) {
		if (err != FAILURE) {
			zval_ptr_dtor(&decoded);
		}
		RETURN_NULL();
	}
	RETURN_COPY_VALUE(&decoded);
}

PHP_METHOD(NetherNet_Client, connect)
{
	char *url;
	size_t url_len;
	HashTable *options = NULL;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_PATH(url, url_len)
		Z_PARAM_OPTIONAL
		Z_PARAM_ARRAY_HT(options)
	ZEND_PARSE_PARAMETERS_END();

	php_nethernet_server *intern = Z_NETHERNET_SERVER_P(ZEND_THIS);
	if (!client_ensure_init(intern)) {
		RETURN_THROWS();
	}

	NetherNetClientConfig cfg = {};
	cfg.server_url = url;
	char *token = NULL, *key_pem = NULL, *network_id = NULL, *idp_domain = NULL, *expected = NULL;
	if (options) {
		token = option_string(options, "token");
		key_pem = option_string(options, "key_pem");
		network_id = option_string(options, "network_id");
		idp_domain = option_string(options, "idp_domain");
		expected = option_string(options, "expected_fingerprint");
		bool allow_missing_identity = false;
		option_bool(options, "allow_missing_identity", &allow_missing_identity);
		cfg.allow_missing_identity = allow_missing_identity;
		zval *v = zend_hash_str_find(options, "timeout_ms", sizeof("timeout_ms") - 1);
		if (v) {
			zend_long timeout_ms = zval_get_long(v);
			cfg.timeout_ms = (int)(timeout_ms < 0 ? 0 : timeout_ms > 600000 ? 600000 : timeout_ms);
		}
	}
	cfg.game_server_token = token;
	cfg.client_key_pem = key_pem;
	cfg.network_id = network_id;
	cfg.idp_domain = idp_domain;
	cfg.expected_fingerprint = expected;

	uint64_t id = 0;
	char fingerprint[96] = {};
	NetherNetResult r = EG(exception) ? NN_INVALID_ARGS : nn_client_connect(&intern->nn, &cfg, &id, fingerprint, sizeof(fingerprint));

	if (token) {
		efree(token);
	}
	if (key_pem) {
		efree(key_pem);
	}
	if (network_id) {
		efree(network_id);
	}
	if (idp_domain) {
		efree(idp_domain);
	}
	if (expected) {
		efree(expected);
	}

	if (EG(exception)) {
		RETURN_THROWS();
	}
	if (r != NN_SUCCESS) {
		zend_throw_exception_ex(nethernet_exception_ce, r, "Unable to connect: %s", result_str(r));
		RETURN_THROWS();
	}
	array_init_size(return_value, 2);
	add_assoc_long(return_value, "id", (zend_long)id);
	add_assoc_string(return_value, "fingerprint", fingerprint);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_server_construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_start, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, url, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, information, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_set_information, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, information, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_void, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_poll_event, 0, 0, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_send, 0, 2, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, id, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, payload, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, channel, IS_LONG, 0, "NetherNet\\Server::CHANNEL_RELIABLE")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_disconnect, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, id, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_bool, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_acl_list, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, entries, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_whitelist_enabled, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, enabled, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_reject_policy, 0, 3, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, banned, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, notWhitelisted, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, whenFull, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_server_capacity, 0, 2, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, players, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, maxPlayers, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_client_query, 0, 1, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO(0, url, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_client_connect, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, url, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

static const zend_function_entry nethernet_client_methods[] = {
	PHP_ME(NetherNet_Client, query, arginfo_client_query, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Client, connect, arginfo_client_connect, ZEND_ACC_PUBLIC)
	PHP_MALIAS(NetherNet_Server, update, update, arginfo_server_void, ZEND_ACC_PUBLIC)
	PHP_MALIAS(NetherNet_Server, pollEvent, pollEvent, arginfo_server_poll_event, ZEND_ACC_PUBLIC)
	PHP_MALIAS(NetherNet_Server, send, send, arginfo_server_send, ZEND_ACC_PUBLIC)
	PHP_MALIAS(NetherNet_Server, disconnect, disconnect, arginfo_server_disconnect, ZEND_ACC_PUBLIC)
	PHP_MALIAS(NetherNet_Server, close, close, arginfo_server_void, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

static const zend_function_entry nethernet_server_methods[] = {
	PHP_ME(NetherNet_Server, __construct, arginfo_server_construct, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, start, arginfo_server_start, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setInformation, arginfo_server_set_information, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, update, arginfo_server_void, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, pollEvent, arginfo_server_poll_event, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, send, arginfo_server_send, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, disconnect, arginfo_server_disconnect, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, close, arginfo_server_void, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, isRunning, arginfo_server_bool, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setBannedNames, arginfo_server_acl_list, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setBannedIps, arginfo_server_acl_list, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setWhitelist, arginfo_server_acl_list, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setWhitelistEnabled, arginfo_server_whitelist_enabled, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setRejectPolicy, arginfo_server_reject_policy, ZEND_ACC_PUBLIC)
	PHP_ME(NetherNet_Server, setCapacity, arginfo_server_capacity, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

static void declare_long_constant(zend_class_entry *ce, const char *name, zend_long value)
{
	zend_declare_class_constant_long(ce, name, strlen(name), value);
}

PHP_MINIT_FUNCTION(nethernet)
{
	zend_class_entry ce;

	INIT_NS_CLASS_ENTRY(ce, "NetherNet", "NetherNetException", NULL);
	nethernet_exception_ce = zend_register_internal_class_ex(&ce, spl_ce_RuntimeException);

	INIT_NS_CLASS_ENTRY(ce, "NetherNet", "Server", nethernet_server_methods);
	nethernet_server_ce = zend_register_internal_class(&ce);
	nethernet_server_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
	nethernet_server_ce->create_object = nethernet_server_create;

	memcpy(&nethernet_server_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	nethernet_server_handlers.offset = XtOffsetOf(php_nethernet_server, std);
	nethernet_server_handlers.free_obj = nethernet_server_free;
	nethernet_server_handlers.clone_obj = NULL;

	INIT_NS_CLASS_ENTRY(ce, "NetherNet", "Client", nethernet_client_methods);
	nethernet_client_ce = zend_register_internal_class(&ce);
	nethernet_client_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE;
	nethernet_client_ce->create_object = nethernet_server_create;

	declare_long_constant(nethernet_server_ce, "CHANNEL_RELIABLE", NN_CHANNEL_RELIABLE);
	declare_long_constant(nethernet_server_ce, "CHANNEL_UNRELIABLE", NN_CHANNEL_UNRELIABLE);

	declare_long_constant(nethernet_server_ce, "EVENT_CONNECTED", NN_EVENT_CONNECTED);
	declare_long_constant(nethernet_server_ce, "EVENT_DATA", NN_EVENT_DATA);
	declare_long_constant(nethernet_server_ce, "EVENT_DISCONNECTED", NN_EVENT_DISCONNECTED);

	declare_long_constant(nethernet_server_ce, "SEND_SUCCESS", NN_SUCCESS);
	declare_long_constant(nethernet_server_ce, "SEND_NOT_FOUND", NN_CONNECTION_NOT_FOUND);
	declare_long_constant(nethernet_server_ce, "SEND_NOT_READY", NN_CONNECTION_NOT_READY);
	declare_long_constant(nethernet_server_ce, "SEND_TOO_LARGE", NN_MESSAGE_TOO_LARGE);
	declare_long_constant(nethernet_server_ce, "SEND_FAILED", NN_SEND_FAIL);
	return SUCCESS;
}

PHP_MINFO_FUNCTION(nethernet)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "nethernet support", "enabled");
	php_info_print_table_row(2, "Version", PHP_NETHERNET_VERSION);
	php_info_print_table_end();
}

static const zend_module_dep nethernet_deps[] = {
	ZEND_MOD_REQUIRED("json")
	ZEND_MOD_REQUIRED("spl")
	ZEND_MOD_END
};

zend_module_entry nethernet_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	nethernet_deps,
	"nethernet",
	NULL,
	PHP_MINIT(nethernet),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(nethernet),
	PHP_NETHERNET_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NETHERNET
ZEND_TSRMLS_CACHE_DEFINE()
ZEND_GET_MODULE(nethernet)
#endif
