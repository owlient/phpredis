/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Nicolas Favre-Felix <n.favre-felix@owlient.eu>               |
  | Maintainer: Michael Grunder <michael.grunder@gmail.com>              |
  +----------------------------------------------------------------------+
*/
#include "redis_array_impl.h"
#include "php_redis.h"
#include "library.h"

#include "php_variables.h"
#include "SAPI.h"
#include "ext/standard/url.h"

#define PHPREDIS_INDEX_NAME    "__phpredis_array_index__"

extern int le_redis_sock;
extern zend_class_entry *redis_ce;

RedisArray*
ra_load_hosts(RedisArray *ra, HashTable *hosts, long retry_interval, zend_bool b_lazy_connect TSRMLS_DC)
{
    int i = 0, host_len;
    zval *id;
    char *host, *p;
    short port;
    zval *zpData, z_cons, z_ret;
    RedisSock *redis_sock  = NULL;

    /* function calls on the Redis object */
    ZVAL_STRING(&z_cons, "__construct");

    /* init connections */
    for (zend_hash_internal_pointer_reset(hosts);
         zend_hash_has_more_elements(hosts) == SUCCESS;
         zend_hash_move_forward(hosts))
    {
        if ((zpData = zend_hash_get_current_data(hosts)) == NULL || (Z_TYPE_P(zpData) != IS_STRING))
        {
            zval_dtor(&z_cons);
            efree(ra);
            return NULL;
        }

        ra->hosts[i] = estrdup(Z_STRVAL_P(zpData));

        /* default values */
        host = Z_STRVAL_P(zpData);
        host_len = Z_STRLEN_P(zpData);
        port = 6379;

		if(((p = strrchr(host, ':')))) { /* found port */
			host_len = p - host;
			port = (short)atoi(p+1);
		} else if(strchr(host,'/') != NULL) { /* unix socket */
		    port = -1;
		}

        /* create Redis object */
        object_init_ex(&ra->redis[i], redis_ce);
        call_user_function(&redis_ce->function_table, &ra->redis[i], &z_cons, &z_ret, 0, NULL TSRMLS_CC);

        /* create socket */
        redis_sock = redis_sock_create(host, host_len, port, ra->connect_timeout, ra->pconnect, NULL, retry_interval, b_lazy_connect);

        if (!b_lazy_connect) {
            /* connect */
            redis_sock_server_open(redis_sock, 1 TSRMLS_CC);
        }

        /* attach */
        id = zend_list_insert(redis_sock, le_redis_sock TSRMLS_CC);
        add_property_resource(&ra->redis[i], "socket", Z_RES_P(id));
        i++;
    }

    /* Cleanup constructor zval */
    zval_dtor(&z_cons);

    return ra;
}

/* List pure functions */
void ra_init_function_table(RedisArray *ra) {
    array_init(&ra->z_pure_cmds);

    add_assoc_bool(&ra->z_pure_cmds, "HGET", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HGETALL", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HKEYS", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HLEN", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SRANDMEMBER", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HMGET", 1);
    add_assoc_bool(&ra->z_pure_cmds, "STRLEN", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SUNION", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HVALS", 1);
    add_assoc_bool(&ra->z_pure_cmds, "TYPE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "EXISTS", 1);
    add_assoc_bool(&ra->z_pure_cmds, "LINDEX", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SCARD", 1);
    add_assoc_bool(&ra->z_pure_cmds, "LLEN", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SDIFF", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZCARD", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZCOUNT", 1);
    add_assoc_bool(&ra->z_pure_cmds, "LRANGE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZRANGE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZRANK", 1);
    add_assoc_bool(&ra->z_pure_cmds, "GET", 1);
    add_assoc_bool(&ra->z_pure_cmds, "GETBIT", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SINTER", 1);
    add_assoc_bool(&ra->z_pure_cmds, "GETRANGE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZREVRANGE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SISMEMBER", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZREVRANGEBYSCORE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZREVRANK", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HEXISTS", 1);
    add_assoc_bool(&ra->z_pure_cmds, "ZSCORE", 1);
    add_assoc_bool(&ra->z_pure_cmds, "HGET", 1);
    add_assoc_bool(&ra->z_pure_cmds, "OBJECT", 1);
    add_assoc_bool(&ra->z_pure_cmds, "SMEMBERS", 1);
}

static int
ra_find_name(const char *name) {

    const char *ini_names, *p, *next;

    ini_names = INI_STR("redis.arrays.names");
    for(p = ini_names; p;) {
        next = strchr(p, ',');
        if(next) {
            if(strncmp(p, name, next - p) == 0) {
                return 1;
            }
        } else {
            if(strcmp(p, name) == 0) {
                return 1;
            }
            break;
        }
        p = next + 1;
    }

    return 0;
}

/* laod array from INI settings */
RedisArray *ra_load_array(const char *name TSRMLS_DC) {

    zval z_params_hosts, *z_hosts;
    zval z_params_prev, *z_prev;
    zval z_params_funs, *z_data_p, z_fun, z_dist;
    zval z_params_index;
    zval z_params_autorehash;
    zval z_params_retry_interval;
    zval z_params_pconnect;
    zval z_params_connect_timeout;
    zval z_params_lazy_connect;
    RedisArray *ra = NULL;

    zend_bool b_index = 0, b_autorehash = 0, b_pconnect = 0;
    long l_retry_interval = 0;
    zend_bool b_lazy_connect = 0;
    double d_connect_timeout = 0;
    HashTable *hHosts = NULL, *hPrev = NULL;

    /* Initialize callbacks to undefined */
    ZVAL_UNDEF(&z_fun);
    ZVAL_UNDEF(&z_dist);

    /* find entry */
    if(!ra_find_name(name))
        return ra;

    /* find hosts */
    array_init(&z_params_hosts);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.hosts")), &z_params_hosts TSRMLS_CC);
    if ((z_hosts = zend_hash_str_find(Z_ARRVAL(z_params_hosts), name, strlen(name))) != NULL) {
        hHosts = Z_ARRVAL_P(z_hosts);
    }

    /* find previous hosts */
    array_init(&z_params_prev);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.previous")), &z_params_prev TSRMLS_CC);
    if ((z_prev = zend_hash_str_find(Z_ARRVAL(z_params_prev), name, strlen(name))) != NULL) {
        hPrev = Z_ARRVAL_P(z_prev);
    }

    /* find function */
    array_init(&z_params_funs);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.functions")), &z_params_funs TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_funs), name, strlen(name))) != NULL) {
        ZVAL_DUP(&z_fun, z_data_p);
    }

    /* find distributor */
    array_init(&z_params_funs);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.distributor")), &z_params_funs TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_funs), name, strlen(name))) != NULL) {
        ZVAL_DUP(&z_dist, z_data_p);
    }

    /* find index option */
    array_init(&z_params_index);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.index")), &z_params_index TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_index), name, strlen(name))) != NULL) {
        if(Z_TYPE_P(z_data_p) == IS_STRING && strncmp(Z_STRVAL_P(z_data_p), "1", 1) == 0) {
            b_index = 1;
        }
    }

    /* find autorehash option */
    array_init(&z_params_autorehash);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.autorehash")), &z_params_autorehash TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_autorehash), name, strlen(name))) != NULL) {
        if(Z_TYPE_P(z_data_p) == IS_STRING && strncmp(Z_STRVAL_P(z_data_p), "1", 1) == 0) {
            b_autorehash = 1;
        }
    }

    /* find retry interval option */
    array_init(&z_params_retry_interval);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.retryinterval")), &z_params_retry_interval TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_retry_interval), name, strlen(name))) != NULL) {
        if (Z_TYPE_P(z_data_p) == IS_LONG || Z_TYPE_P(z_data_p) == IS_STRING) {
            if (Z_TYPE_P(z_data_p) == IS_LONG) {
                l_retry_interval = Z_LVAL_P(z_data_p);
            }
            else {
                l_retry_interval = atol(Z_STRVAL_P(z_data_p));
            }
        }
    }

    /* find pconnect option */
    array_init(&z_params_pconnect);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.pconnect")), &z_params_pconnect TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_pconnect), name, strlen(name))) != NULL) {
        if(Z_TYPE_P(z_data_p) == IS_STRING && strncmp(Z_STRVAL_P(z_data_p), "1", 1) == 0) {
            b_pconnect = 1;
        }
    }

    /* find lazy connect option */
    array_init(&z_params_lazy_connect);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.lazyconnect")), &z_params_lazy_connect TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_lazy_connect), name, strlen(name))) != NULL) {
        if(Z_TYPE_P(z_data_p) == IS_STRING && strncmp(Z_STRVAL_P(z_data_p), "1", 1) == 0) {
            b_lazy_connect = 1;
        }
    }

    /* find connect timeout option */
    array_init(&z_params_connect_timeout);
    sapi_module.treat_data(PARSE_STRING, estrdup(INI_STR("redis.arrays.connecttimeout")), &z_params_connect_timeout TSRMLS_CC);
    if ((z_data_p = zend_hash_str_find(Z_ARRVAL(z_params_connect_timeout), name, strlen(name))) != NULL) {
        if (Z_TYPE_P(z_data_p) == IS_DOUBLE ||
            Z_TYPE_P(z_data_p) == IS_STRING ||
            Z_TYPE_P(z_data_p) == IS_LONG)
        {
        if (Z_TYPE_P(z_data_p) == IS_DOUBLE) {
            d_connect_timeout = Z_DVAL_P(z_data_p);
        } else if (Z_TYPE_P(z_data_p) == IS_LONG) {
            d_connect_timeout = Z_LVAL_P(z_data_p);
        } else {
            d_connect_timeout = atof(Z_STRVAL_P(z_data_p));
        }
        }
    }

    /* create RedisArray object */
    ra = ra_make_array(hHosts, &z_fun, &z_dist, hPrev, b_index, b_pconnect, l_retry_interval, b_lazy_connect, d_connect_timeout TSRMLS_CC);
    ra->auto_rehash = b_autorehash;
    if(ra->prev) ra->prev->auto_rehash = b_autorehash;

    /* cleanup */
    zval_dtor(&z_params_hosts);
    zval_dtor(&z_params_prev);
    zval_dtor(&z_params_funs);
    zval_dtor(&z_params_index);
    zval_dtor(&z_params_autorehash);
    zval_dtor(&z_params_retry_interval);
    zval_dtor(&z_params_pconnect);
    zval_dtor(&z_params_connect_timeout);
    zval_dtor(&z_params_lazy_connect);

    return ra;
}

RedisArray *
ra_make_array(HashTable *hosts, zval *z_fun, zval *z_dist, HashTable *hosts_prev,
              zend_bool b_index, zend_bool b_pconnect, long retry_interval,
              zend_bool b_lazy_connect, double connect_timeout TSRMLS_DC)
{
    int count = zend_hash_num_elements(hosts);

    /* create object */
    RedisArray *ra = emalloc(sizeof(RedisArray));
    ra->hosts = emalloc(count * sizeof(char*));
    ra->redis = ecalloc(count, sizeof(zval));
    ra->count = count;
    ra->index = b_index;
    ra->auto_rehash = 0;
    ra->pconnect = b_pconnect;
    ra->connect_timeout = connect_timeout;
    ra->z_multi_exec = NULL;
    ra->prev = NULL;

    /* init array data structures */
    ra_init_function_table(ra);

    if(NULL == ra_load_hosts(ra, hosts, retry_interval, b_lazy_connect TSRMLS_CC)) {
        return NULL;
    }

    if (hosts_prev) {
        ra->prev = ra_make_array(hosts_prev, z_fun, z_dist, NULL, b_index,
                                 b_pconnect, retry_interval, b_lazy_connect,
                                 connect_timeout TSRMLS_CC);
    }

    /* Set hash function and distribtor if provided */
    if (z_fun != NULL) {
        ZVAL_DUP(&ra->z_fun, z_fun);
    }
    if (z_dist != NULL) {
        ZVAL_DUP(&ra->z_dist, z_dist);
    }

    return ra;
}


/* call userland key extraction function */
char *
ra_call_extractor(RedisArray *ra, const char *key, int key_len, int *out_len TSRMLS_DC) {

    char *out;
    zval z_ret;
    zval z_argv0;

    /* check that we can call the extractor function */
    if(!zend_is_callable_ex(&ra->z_fun, NULL, 0, NULL, NULL, NULL)) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not call extractor function");
        return NULL;
    }
    /* convert_to_string(ra->z_fun); */

    /* call extraction function */
    ZVAL_STRINGL(&z_argv0, key, key_len);
    call_user_function(EG(function_table), NULL, &ra->z_fun, &z_ret, 1, &z_argv0 TSRMLS_CC);

    if(Z_TYPE(z_ret) != IS_STRING) {
        zval_dtor(&z_ret);
        return NULL;
    }

    *out_len = Z_STRLEN(z_ret);
    out = emalloc(*out_len + 1);
    out[*out_len] = 0;
    memcpy(out, Z_STRVAL(z_ret), *out_len);

    zval_dtor(&z_argv0);
    zval_dtor(&z_ret);
    return out;
}

static char *
ra_extract_key(RedisArray *ra, const char *key, int key_len, int *out_len TSRMLS_DC) {

    char *start, *end, *out;
    *out_len = key_len;

    if(Z_TYPE(ra->z_fun) != IS_UNDEF)
        return ra_call_extractor(ra, key, key_len, out_len TSRMLS_CC);

    /* look for '{' */
    start = strchr(key, '{');
    if(!start) return estrndup(key, key_len);

    /* look for '}' */
    end = strchr(start + 1, '}');
    if(!end) return estrndup(key, key_len);

    /* found substring */
    *out_len = end - start - 1;
    out = emalloc(*out_len + 1);
    out[*out_len] = 0;
    memcpy(out, start+1, *out_len);

    return out;
}

/* call userland key distributor function */
zend_bool
ra_call_distributor(RedisArray *ra, const char *key, int key_len, int *pos TSRMLS_DC) {

    zval z_ret;
    zval z_argv0;

    /* check that we can call the extractor function */
    if(!zend_is_callable_ex(&ra->z_dist, NULL, 0, NULL, NULL, NULL)) {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Could not call distributor function");
        return 0;
    }
    /* convert_to_string(ra->z_fun); */

    /* call extraction function */
    ZVAL_STRINGL(&z_argv0, key, key_len);
    call_user_function(EG(function_table), NULL, &ra->z_dist, &z_ret, 1, &z_argv0 TSRMLS_CC);

    if(Z_TYPE(z_ret) != IS_LONG) {
        zval_dtor(&z_ret);
        return 0;
    }

    zval_dtor(&z_argv0);

    *pos = Z_LVAL(z_ret);
    zval_dtor(&z_ret);
    return 1;
}

zval *
ra_find_node(RedisArray *ra, const char *key, int key_len, int *out_pos TSRMLS_DC) {
    uint32_t hash;
    char *out;
    int pos, out_len;

    /* extract relevant part of the key */
    out = ra_extract_key(ra, key, key_len, &out_len TSRMLS_CC);
    if(!out) return NULL;

    if(Z_TYPE(ra->z_dist) != IS_UNDEF) {
        if (!ra_call_distributor(ra, key, key_len, &pos TSRMLS_CC)) {
            efree(out);
            return NULL;
        }
        efree(out);
    } else {
        uint64_t h64;

        /* hash */
        hash = rcrc32(out, out_len);
        efree(out);

        /* get position on ring */
        h64 = hash;
        h64 *= ra->count;
        h64 /= 0xffffffff;
        pos = (int)h64;
    }
    if(out_pos) *out_pos = pos;

    return &ra->redis[pos];
}

zval *
ra_find_node_by_name(RedisArray *ra, const char *host, int host_len TSRMLS_DC) {

    int i;
    for(i = 0; i < ra->count; ++i) {
        if(strncmp(ra->hosts[i], host, host_len) == 0) {
            return &ra->redis[i];
        }
    }
    return NULL;
}


char *
ra_find_key(RedisArray *ra, zval *z_args, const char *cmd, int *key_len) {

    zval *zp_tmp;
    int key_pos = 0; /* TODO: change this depending on the command */

    if( zend_hash_num_elements(Z_ARRVAL_P(z_args)) == 0 ||
        (zp_tmp = zend_hash_index_find(Z_ARRVAL_P(z_args), key_pos)) == NULL ||
        Z_TYPE_P(zp_tmp) != IS_STRING) {

        return NULL;
    }

    *key_len = Z_STRLEN_P(zp_tmp);
    return Z_STRVAL_P(zp_tmp);
}

void
ra_index_multi(zval *z_redis, long multi_value TSRMLS_DC) {
    zval z_fun_multi, z_ret;
    zval z_args[1];

    /* run MULTI */
    ZVAL_STRING(&z_fun_multi, "MULTI");
    ZVAL_LONG(&z_args[0], multi_value);

    call_user_function(&redis_ce->function_table, z_redis, &z_fun_multi, &z_ret, 1, z_args TSRMLS_CC);

    /* Clean up allocated string and return value */
    zval_dtor(&z_fun_multi);
    zval_dtor(&z_ret);
}

static void
ra_index_change_keys(const char *cmd, zval *z_keys, zval *z_redis TSRMLS_DC) {

    int i, argc;
    zval z_fun, z_ret, *z_args;

    /* alloc */
    argc = 1 + zend_hash_num_elements(Z_ARRVAL_P(z_keys));
    z_args = emalloc(argc * sizeof(zval));

    /* prepare first parameters */
    ZVAL_STRING(&z_fun, cmd);
    ZVAL_STRING(&z_args[0], PHPREDIS_INDEX_NAME);

    /* prepare keys */
    for(i = 0; i < argc - 1; ++i) {
        zval *zp;
        zp = zend_hash_index_find(Z_ARRVAL_P(z_keys), i);
        ZVAL_DUP(&z_args[i+1], zp);
    }

    /* run cmd */
    call_user_function(&redis_ce->function_table, z_redis, &z_fun, &z_ret, argc, z_args TSRMLS_CC);

    /* cleanup */
    for (i = 0; i < argc; i++) {
        zval_dtor(&z_args[i]);
    }
    efree(z_args);
    zval_dtor(&z_ret);
    zval_dtor(&z_fun);
}

void
ra_index_del(zval *z_keys, zval *z_redis TSRMLS_DC) {
    ra_index_change_keys("SREM", z_keys, z_redis TSRMLS_CC);
}

void
ra_index_keys(zval *z_pairs, zval *z_redis TSRMLS_DC) {
    /* Initialize key array */
    zval z_keys, *z_entry_p;
    HashPosition pos;
    array_init_size(&z_keys, zend_hash_num_elements(Z_ARRVAL_P(z_pairs)));

    /* Go through input array and add values to the key array */
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(z_pairs), &pos);
    while ((z_entry_p = zend_hash_get_current_data_ex(Z_ARRVAL_P(z_pairs), &pos)) != NULL) {
            zend_string *key;
            zend_ulong num_key;
            zval z_new;

            switch (zend_hash_get_current_key_ex(Z_ARRVAL_P(z_pairs), &key, &num_key, &pos)) {
                case HASH_KEY_IS_STRING:
                    ZVAL_STRINGL(&z_new, key->val, key->len);
                    zend_hash_next_index_insert(Z_ARRVAL(z_keys), &z_new);
                    break;

                case HASH_KEY_IS_LONG:
                    ZVAL_LONG(&z_new, num_key);
                    zend_hash_next_index_insert(Z_ARRVAL(z_keys), &z_new);
                    break;
            }
            zend_hash_move_forward_ex(Z_ARRVAL_P(z_pairs), &pos);
    }

    /* add keys to index */
    ra_index_change_keys("SADD", &z_keys, z_redis TSRMLS_CC);

    /* cleanup */
    zval_dtor(&z_keys);
}

void
ra_index_key(const char *key, int key_len, zval *z_redis TSRMLS_DC) {

    zval z_fun_sadd, z_ret, z_args[2];

    /* prepare args */
    ZVAL_STRINGL(&z_fun_sadd, "SADD", 4);

    ZVAL_STRING(&z_args[0], PHPREDIS_INDEX_NAME);
    ZVAL_STRINGL(&z_args[1], key, key_len);

    /* run SADD */
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_sadd, &z_ret, 2, z_args TSRMLS_CC);

    zval_dtor(&z_ret);
    zval_dtor(&z_fun_sadd);
    zval_dtor(&z_args[0]);
    zval_dtor(&z_args[1]);
}

void
ra_index_exec(zval *z_redis, zval *return_value, int keep_all TSRMLS_DC) {
    zval z_fun_exec, z_ret, *zp_tmp;

    /* run EXEC */
    ZVAL_STRING(&z_fun_exec, "EXEC");
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_exec, &z_ret, 0, NULL TSRMLS_CC);
    zval_dtor(&z_fun_exec);

    /* extract first element of exec array and put into return_value. */
    if(Z_TYPE(z_ret) == IS_ARRAY) {
        if(return_value) {
            if(keep_all) {
                ZVAL_DUP(return_value, &z_ret);
            } else if((zp_tmp = zend_hash_index_find(Z_ARRVAL(z_ret), 0)) != NULL) {
                ZVAL_DUP(return_value, zp_tmp);
            }
        }
        zval_dtor(&z_ret);
    }
}

void
ra_index_discard(zval *z_redis, zval *return_value TSRMLS_DC) {
    zval z_fun_discard, z_ret;

    /* run DISCARD */
    ZVAL_STRING(&z_fun_discard, "DISCARD");

    call_user_function(&redis_ce->function_table, z_redis, &z_fun_discard, &z_ret, 0, NULL TSRMLS_CC);

    zval_dtor(&z_fun_discard);
    zval_dtor(&z_ret);
}

void
ra_index_unwatch(zval *z_redis, zval *return_value TSRMLS_DC) {

    zval z_fun_unwatch, z_ret;

    /* run UNWATCH */
    ZVAL_STRING(&z_fun_unwatch, "UNWATCH");
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_unwatch, &z_ret, 0, NULL TSRMLS_CC);

    zval_dtor(&z_ret);
}

zend_bool
ra_is_write_cmd(RedisArray *ra, const char *cmd, int cmd_len) {

    zend_bool ret;
    int i;
    char *cmd_up = emalloc(1 + cmd_len);

    /* convert to uppercase */
    for(i = 0; i < cmd_len; ++i) {
        cmd_up[i] = toupper(cmd[i]);
    }
    cmd_up[cmd_len] = 0;

    ret = zend_hash_str_exists(Z_ARRVAL(ra->z_pure_cmds), cmd_up, cmd_len);

    efree(cmd_up);
    return !ret;
}

/* list keys from array index */
static long
ra_rehash_scan(zval *z_redis, char ***keys, int **key_lens, const char *cmd, const char *arg TSRMLS_DC) {

    long count, i;
    zval z_fun_smembers, z_ret, z_arg, *z_data_p;
    HashTable *h_keys;
    HashPosition pointer;
    char *key;
    int key_len;

    /* Function and argument */
    ZVAL_STRING(&z_fun_smembers, cmd);
    ZVAL_STRING(&z_arg, arg);

    /* run SMEMBERS */
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_smembers, &z_ret, 1, &z_arg TSRMLS_CC);
    if(Z_TYPE(z_ret) != IS_ARRAY) { /* failure */
        return -1;    /* TODO: log error. */
    }
    h_keys = Z_ARRVAL(z_ret);

    /* allocate key array */
    count = zend_hash_num_elements(h_keys);
    *keys = emalloc(count * sizeof(char*));
    *key_lens = emalloc(count * sizeof(int));

    for (i = 0, zend_hash_internal_pointer_reset_ex(h_keys, &pointer);
            (z_data_p = zend_hash_get_current_data_ex(h_keys, &pointer)) != NULL;
            zend_hash_move_forward_ex(h_keys, &pointer), ++i) {

        key = Z_STRVAL_P(z_data_p);
        key_len = Z_STRLEN_P(z_data_p);

        /* copy key and length */
        (*keys)[i] = emalloc(1 + key_len);
        memcpy((*keys)[i], key, key_len);
        (*key_lens)[i] = key_len;
        (*keys)[i][key_len] = 0; /* null-terminate string */
    }

    /* cleanup */
    zval_dtor(&z_fun_smembers);
    zval_dtor(&z_arg);
    zval_dtor(&z_ret);

    return count;
}

static long
ra_rehash_scan_index(zval *z_redis, char ***keys, int **key_lens TSRMLS_DC) {
    return ra_rehash_scan(z_redis, keys, key_lens, "SMEMBERS", PHPREDIS_INDEX_NAME TSRMLS_CC);
}

/* list keys using KEYS command */
static long
ra_rehash_scan_keys(zval *z_redis, char ***keys, int **key_lens TSRMLS_DC) {
    return ra_rehash_scan(z_redis, keys, key_lens, "KEYS", "*" TSRMLS_CC);
}

/* run TYPE to find the type */
static zend_bool
ra_get_key_type(zval *z_redis, const char *key, int key_len, zval *z_from, long *res TSRMLS_DC) {
    int i;
    zval z_fun_type, z_ret, z_arg;
    zval *z_data;
    long success = 1;

    /* Pipelined */
    ra_index_multi(z_from, PIPELINE TSRMLS_CC);

    /* Run TYPE on our key */
    ZVAL_STRINGL(&z_fun_type, "TYPE", 4);
    ZVAL_STRINGL(&z_arg, key, key_len);
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_type, &z_ret, 1, &z_arg TSRMLS_CC);

    /* Cleanup type args */
    zval_dtor(&z_fun_type);
    zval_dtor(&z_arg);
    zval_dtor(&z_ret);

    /* Run TTL on our key */
    ZVAL_STRINGL(&z_fun_type, "TTL", 3);
    ZVAL_STRINGL(&z_arg, key, key_len);
    call_user_function(&redis_ce->function_table, z_redis, &z_fun_type, &z_ret, 1, &z_arg TSRMLS_CC);

    /* Cleanup TTL args */
    zval_dtor(&z_fun_type);
    zval_dtor(&z_arg);
    zval_dtor(&z_ret);

    /* Get the result from the pipeline. */
    ra_index_exec(z_from, &z_ret, 1 TSRMLS_CC);

    if(Z_TYPE(z_ret) == IS_ARRAY) {
        HashTable *retHash = Z_ARRVAL(z_ret);
        for(i = 0, zend_hash_internal_pointer_reset(retHash);
                zend_hash_has_more_elements(retHash) == SUCCESS;
                zend_hash_move_forward(retHash)) {

            if((z_data = zend_hash_get_current_data(retHash)) == NULL) {
                success = 0;
                break;
            }
            if(Z_TYPE_P(z_data) != IS_LONG) {
                success = 0;
                break;

            }
            res[i] = Z_LVAL_P(z_data);
            i++;
        }
    }
    zval_dtor(&z_ret);

    return success;
}

/* delete key from source server index during rehashing */
static void
ra_remove_from_index(zval *z_redis, const char *key, int key_len TSRMLS_DC) {

    zval z_fun_srem, z_ret, z_args[2];

    /* run SREM on source index */
    ZVAL_STRINGL(&z_fun_srem, "SREM", 4);
    ZVAL_STRING(&z_args[0], PHPREDIS_INDEX_NAME);
    ZVAL_STRINGL(&z_args[1], key, key_len);

    call_user_function(&redis_ce->function_table, z_redis, &z_fun_srem, &z_ret, 2, z_args TSRMLS_CC);

    zval_dtor(&z_fun_srem);
    zval_dtor(&z_args[0]);
    zval_dtor(&z_args[1]);
    zval_dtor(&z_ret);
}


/* delete key from source server during rehashing */
static zend_bool
ra_del_key(const char *key, int key_len, zval *z_from TSRMLS_DC) {
    zval z_fun_del, z_ret, z_args;

    /* in a transaction */
    ra_index_multi(z_from, MULTI TSRMLS_CC);

    /* run DEL on source */
    ZVAL_STRINGL(&z_fun_del, "DEL", 3);
    ZVAL_STRINGL(&z_args, key, key_len);
    call_user_function(&redis_ce->function_table, z_from, &z_fun_del, &z_ret, 1, &z_args TSRMLS_CC);

    /* Cleanup DEL and key */
    zval_dtor(&z_fun_del);
    zval_dtor(&z_args);
    zval_dtor(&z_ret);

    /* remove key from index */
    ra_remove_from_index(z_from, key, key_len TSRMLS_CC);

    /* close transaction */
    ra_index_exec(z_from, NULL, 0 TSRMLS_CC);

    return 1;
}

static zend_bool
ra_expire_key(const char *key, int key_len, zval *z_to, long ttl TSRMLS_DC) {

    zval z_fun_expire, z_ret, z_args[2];

    if (ttl > 0)
    {
        /* run EXPIRE on target */
        ZVAL_STRINGL(&z_fun_expire, "EXPIRE", 6);
        ZVAL_STRINGL(&z_args[0], key, key_len);
        ZVAL_LONG(&z_args[1], ttl);
        call_user_function(&redis_ce->function_table, z_to, &z_fun_expire, &z_ret, 2, z_args TSRMLS_CC);
    }

    return 1;
}

static zend_bool
ra_move_zset(const char *key, int key_len, zval *z_from, zval *z_to, long ttl TSRMLS_DC) {
    zval z_fun_zrange, z_fun_zadd, z_ret, z_ret_dest, z_args[4], *z_score_p, *z_zadd_args;
    int count;
    HashTable *h_zset_vals;
    zend_string *val;
    int i;
    zend_ulong idx;

    /* run ZRANGE key 0 -1 WITHSCORES on source */
    ZVAL_STRINGL(&z_fun_zrange, "ZRANGE", 6);
    ZVAL_STRINGL(&z_args[0], key, key_len);
    ZVAL_STRINGL(&z_args[1], "0", 1);
    ZVAL_STRINGL(&z_args[2], "-1", 2);
    ZVAL_BOOL(&z_args[3], 1);
    call_user_function(&redis_ce->function_table, z_from, &z_fun_zrange, &z_ret, 4, z_args TSRMLS_CC);

    /* cleanup zrange args */
    zval_dtor(&z_fun_zrange);
    for(i = 0; i < 4; ++i) {
        zval_dtor(&z_args[i]); /* FIXME */
    }

    if(Z_TYPE(z_ret) != IS_ARRAY) { /* key not found or replaced */
        /* TODO: report? */
        return 0;
    }

    /* we now have an array of value → score pairs in z_ret. */
    h_zset_vals = Z_ARRVAL(z_ret);

    /* allocate argument array for ZADD */
    count = zend_hash_num_elements(h_zset_vals);
    z_zadd_args = emalloc((1 + 2*count) * sizeof(*z_zadd_args));

    for(i = 1, zend_hash_internal_pointer_reset(h_zset_vals);
            zend_hash_has_more_elements(h_zset_vals) == SUCCESS;
            zend_hash_move_forward(h_zset_vals)) {

        if((z_score_p = zend_hash_get_current_data(h_zset_vals)) == NULL) {
            continue;
        }

        /* add score */
        convert_to_double(z_score_p);
        ZVAL_DOUBLE(&z_zadd_args[i], Z_DVAL_P(z_score_p));

        /* add value */
        switch (zend_hash_get_current_key(h_zset_vals, &val, &idx)) {
            case HASH_KEY_IS_STRING:
                ZVAL_STRINGL(&z_zadd_args[i+1], val->val, val->len);
                break;
            case HASH_KEY_IS_LONG:
                ZVAL_LONG(&z_zadd_args[i+1], (long)idx);
                break;
            default:
                return -1; /* Todo: log error */
                break;
        }
        i += 2;
    }

    /* run ZADD on target */
    ZVAL_STRINGL(&z_fun_zadd, "ZADD", 4);
    ZVAL_STRINGL(&z_zadd_args[0], key, key_len);
    call_user_function(&redis_ce->function_table, z_to, &z_fun_zadd, &z_ret_dest,
        1 + 2 * count, z_zadd_args TSRMLS_CC);

    /* Expire if needed */
    ra_expire_key(key, key_len, z_to, ttl TSRMLS_CC);

    /* cleanup */
    for(i = 0; i < 1 + 2 * count; ++i) {
        zval_dtor(&z_zadd_args[i]);
    }

    zval_dtor(&z_fun_zadd);
    zval_dtor(&z_ret);
    zval_dtor(&z_ret_dest);
    efree(z_zadd_args);

    return 1;
}

static zend_bool
ra_move_string(const char *key, int key_len, zval *z_from, zval *z_to, long ttl TSRMLS_DC) {
    zval z_fun_get, z_fun_set, z_ret, z_args[3];
    int i, argc = 0;

    /* run GET on source */
    ZVAL_STRINGL(&z_fun_get, "GET", 3);
    ZVAL_STRINGL(&z_args[0], key, key_len);
    call_user_function(&redis_ce->function_table, z_from, &z_fun_get, &z_ret, 1, z_args TSRMLS_CC);

    zval_dtor(&z_fun_get);
    zval_dtor(&z_args[0]);

    if(Z_TYPE(z_ret) != IS_STRING) { /* key not found or replaced */
        /* TODO: report? */
        return 0;
    }

    /* run SET on target */
    if (ttl > 0) {
        ZVAL_STRINGL(&z_fun_set, "SETEX", 5);
        ZVAL_STRINGL(&z_args[argc++], key, key_len);
        ZVAL_LONG(&z_args[argc++], ttl);
        ZVAL_STRINGL(&z_args[argc++], Z_STRVAL(z_ret), Z_STRLEN(z_ret)); /* copy z_ret to arg 1 */
    } else {
        ZVAL_STRINGL(&z_fun_set, "SET", 3);
        ZVAL_STRINGL(&z_args[argc++], key, key_len);
        ZVAL_STRINGL(&z_args[argc++], Z_STRVAL(z_ret), Z_STRLEN(z_ret)); /* copy z_ret to arg 1 */
    }

    /* Cleanup get return */
    zval_dtor(&z_ret);

    /* Forward the call */
    call_user_function(&redis_ce->function_table, z_to, &z_fun_set, &z_ret, argc, z_args TSRMLS_CC);

    /* Cleanup */
    zval_dtor(&z_fun_set);
    zval_dtor(&z_ret);

    for (i = 0; i < argc; i++) {
       zval_dtor(&z_args[i]);
    }

    return 1;
}

static zend_bool
ra_move_hash(const char *key, int key_len, zval *z_from, zval *z_to, long ttl TSRMLS_DC) {
    zval z_fun_hgetall, z_fun_hmset, z_ret, z_ret_dest, z_args[2];

    /* run HGETALL on source */
    ZVAL_STRINGL(&z_fun_hgetall, "HGETALL", 7);
    ZVAL_STRINGL(&z_args[0], key, key_len);
    call_user_function(&redis_ce->function_table, z_from, &z_fun_hgetall, &z_ret, 1, z_args TSRMLS_CC);

    /* Cleanup call args */
    zval_dtor(&z_fun_hgetall);
    zval_dtor(&z_args[0]);

    if(Z_TYPE(z_ret) != IS_ARRAY) { /* key not found or replaced */
        /* TODO: report? */
        return 0;
    }

    /* run HMSET on target */
    ZVAL_STRINGL(&z_fun_hmset, "HMSET", 5);
    ZVAL_STRINGL(&z_args[0], key, key_len);
    ZVAL_DUP(&z_args[1], &z_ret);
    call_user_function(&redis_ce->function_table, z_to, &z_fun_hmset, &z_ret_dest, 2, z_args TSRMLS_CC);

    /* Cleanup call args */
    zval_dtor(&z_fun_hmset);
    zval_dtor(&z_args[0]);
    zval_dtor(&z_args[1]);

    /* Expire if needed */
    ra_expire_key(key, key_len, z_to, ttl TSRMLS_CC);

    /* cleanup */
    zval_dtor(&z_ret);
    zval_dtor(&z_ret_dest);

    return 1;
}

static zend_bool
ra_move_collection(const char *key, int key_len, zval *z_from, zval *z_to,
        int list_count, const char **cmd_list,
        int add_count, const char **cmd_add, long ttl TSRMLS_DC) {
    zval z_fun_retrieve, z_fun_sadd, z_ret, *z_data_p, *z_retrieve_args, *z_sadd_args;
    int count, i;
    HashTable *h_set_vals;

    /* run retrieval command on source */
    z_retrieve_args = emalloc((1+list_count) * sizeof(*z_retrieve_args));
    ZVAL_STRING(&z_fun_retrieve, cmd_list[0]);    /* set the command */

    /* set the key */
    ZVAL_STRINGL(&z_retrieve_args[0], key, key_len);

    /* possibly add some other args if they were provided. */
    for(i = 1; i < list_count; ++i) {
        ZVAL_STRING(&z_retrieve_args[i], cmd_list[i]);
    }

    call_user_function(&redis_ce->function_table, z_from, &z_fun_retrieve, &z_ret,
        list_count, z_retrieve_args TSRMLS_CC);

    if(Z_TYPE(z_ret) != IS_ARRAY) { /* key not found or replaced */
        /* TODO: report? */
        return 0;
    }

    /* run SADD/RPUSH on target */
    h_set_vals = Z_ARRVAL(z_ret);
    count = zend_hash_num_elements(h_set_vals);
    z_sadd_args = emalloc((1 + count) * sizeof(*z_sadd_args));
    ZVAL_STRING(&z_fun_sadd, cmd_add[0]);
    ZVAL_STRINGL(&z_sadd_args[0], key, key_len);

    for(i = 0, zend_hash_internal_pointer_reset(h_set_vals);
            zend_hash_has_more_elements(h_set_vals) == SUCCESS;
            zend_hash_move_forward(h_set_vals), i++) {

        if((z_data_p = zend_hash_get_current_data(h_set_vals)) == NULL) {
            continue;
        }

        /* add set elements */
        ZVAL_DUP(&z_sadd_args[i+1], z_data_p);
    }

    /* Clean up our input return value */
    zval_dtor(&z_ret);

    call_user_function(&redis_ce->function_table, z_to, &z_fun_sadd, &z_ret, count+1, z_sadd_args TSRMLS_CC);

    /* Expire if needed */
    ra_expire_key(key, key_len, z_to, ttl TSRMLS_CC);

    /* cleanup */
    for(i = 0; i <= count; ++i) {
        zval_dtor(&z_sadd_args[i]);
    }
    efree(z_sadd_args);

    for (i = 0; i < list_count; i++) {
        zval_dtor(&z_retrieve_args[i]);
    }
    efree(z_retrieve_args);

    /* Clean up our output return value */
    zval_dtor(&z_fun_retrieve);
    zval_dtor(&z_fun_sadd);
    zval_dtor(&z_ret);

    return 1;
}

static zend_bool
ra_move_set(const char *key, int key_len, zval *z_from, zval *z_to, long ttl TSRMLS_DC) {
    const char *cmd_list[] = {"SMEMBERS"};
    const char *cmd_add[] = {"SADD"};
    return ra_move_collection(key, key_len, z_from, z_to, 1, cmd_list, 1, cmd_add, ttl TSRMLS_CC);
}

static zend_bool
ra_move_list(const char *key, int key_len, zval *z_from, zval *z_to, long ttl TSRMLS_DC) {
    const char *cmd_list[] = {"LRANGE", "0", "-1"};
    const char *cmd_add[] = {"RPUSH"};
    return ra_move_collection(key, key_len, z_from, z_to, 3, cmd_list, 1, cmd_add, ttl TSRMLS_CC);
}

void
ra_move_key(const char *key, int key_len, zval *z_from, zval *z_to TSRMLS_DC) {
    long res[2], type, ttl;
    zend_bool success = 0;

    if (ra_get_key_type(z_from, key, key_len, z_from, res TSRMLS_CC)) {
        type = res[0];
        ttl = res[1];

        /* open transaction on target server */
        ra_index_multi(z_to, MULTI TSRMLS_CC);
        switch(type) {
            case REDIS_STRING:
                success = ra_move_string(key, key_len, z_from, z_to, ttl TSRMLS_CC);
                break;

            case REDIS_SET:
                success = ra_move_set(key, key_len, z_from, z_to, ttl TSRMLS_CC);
                break;

            case REDIS_LIST:
                success = ra_move_list(key, key_len, z_from, z_to, ttl TSRMLS_CC);
                break;

            case REDIS_ZSET:
                success = ra_move_zset(key, key_len, z_from, z_to, ttl TSRMLS_CC);
                break;

            case REDIS_HASH:
                success = ra_move_hash(key, key_len, z_from, z_to, ttl TSRMLS_CC);
                break;

            default:
                /* TODO: report? */
                break;
        }
    }

    if(success) {
        ra_del_key(key, key_len, z_from TSRMLS_CC);
        ra_index_key(key, key_len, z_to TSRMLS_CC);
    }

    /* close transaction */
    ra_index_exec(z_to, NULL, 0 TSRMLS_CC);
}

/* callback with the current progress, with hostname and count */
static void zval_rehash_callback(zend_fcall_info *z_cb, zend_fcall_info_cache *z_cb_cache,
    const char *hostname, long count TSRMLS_DC) {

    zval z_ret, z_args[2];

    z_cb->retval = &z_ret;
    z_cb->params = z_args;
    z_cb->param_count = 2;
    z_cb->no_separation = 0;

    /* run cb(hostname, count) */
    ZVAL_STRING(&z_args[0], hostname);
    ZVAL_LONG(&z_args[1], count);

    zend_call_function(z_cb, z_cb_cache TSRMLS_CC);

    zval_dtor(&z_args[0]);
}

static void
ra_rehash_server(RedisArray *ra, zval *z_redis, const char *hostname, zend_bool b_index,
        zend_fcall_info *z_cb, zend_fcall_info_cache *z_cb_cache TSRMLS_DC) {
    char **keys;
    int *key_lens;
    long count, i;
    int target_pos;
    zval *z_target;

    /* list all keys */
    if(b_index) {
        count = ra_rehash_scan_index(z_redis, &keys, &key_lens TSRMLS_CC);
    } else {
        count = ra_rehash_scan_keys(z_redis, &keys, &key_lens TSRMLS_CC);
    }

    /* callback */
    if(z_cb && z_cb_cache) {
        zval_rehash_callback(z_cb, z_cb_cache, hostname, count TSRMLS_CC);
    }

    /* for each key, redistribute */
    for(i = 0; i < count; ++i) {

        /* check that we're not moving to the same node. */
        z_target = ra_find_node(ra, keys[i], key_lens[i], &target_pos TSRMLS_CC);

        if(strcmp(hostname, ra->hosts[target_pos])) { /* different host */
            ra_move_key(keys[i], key_lens[i], z_redis, z_target TSRMLS_CC);
        }
    }

    /* cleanup */
    for(i = 0; i < count; ++i) {
        efree(keys[i]);
    }
    efree(keys);
    efree(key_lens);
}

void
ra_rehash(RedisArray *ra, zend_fcall_info *z_cb, zend_fcall_info_cache *z_cb_cache TSRMLS_DC) {
    int i;

    /* redistribute the data, server by server. */
    if(!ra->prev)
        return;    /* TODO: compare the two rings for equality */

    for(i = 0; i < ra->prev->count; ++i) {
        ra_rehash_server(ra, &ra->prev->redis[i], ra->prev->hosts[i], ra->index, z_cb, z_cb_cache TSRMLS_CC);
    }
}

/* vim: set tabstop=4 softtabstop=4 noexpandtab shiftwidth=4: */
