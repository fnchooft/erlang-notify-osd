/* Copyright (c) 2010-2018, Michael Santos <michael.santos@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the author nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include <libnotify/notify.h>

#include "erl_nif.h"


static ERL_NIF_TERM atom_ok;
static ERL_NIF_TERM atom_error;
static ERL_NIF_TERM atom_nomem;
static ERL_NIF_TERM atom_undefined;

static gchar *stralloc(ErlNifBinary *bin);
int notify_hints_type(ErlNifEnv *env, NotifyNotification *notify, int arity, ERL_NIF_TERM key, ERL_NIF_TERM value);


    static int
load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info)
{
    char buf[1024] = {0};

    if (enif_get_string(env, load_info, buf, sizeof(buf), ERL_NIF_LATIN1) < 1)
        return -1;

    if (!notify_init(buf))
        return -1;

    atom_ok = enif_make_atom(env, "ok");
    atom_error = enif_make_atom(env, "error");
    atom_nomem = enif_make_atom(env, "enomem");
    atom_undefined = enif_make_atom(env, "undefined");

    return 0;
}

    void
unload(ErlNifEnv *env, void *priv_data)
{
    notify_uninit();
}


    static ERL_NIF_TERM
nif_notify(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    ErlNifBinary summary;
    ErlNifBinary body;
    ErlNifBinary icon;

    ErlNifBinary category;
    int urgency = NOTIFY_URGENCY_NORMAL;
    int timeout = 0;

    gchar *s_summary = NULL;
    gchar *s_body = NULL;
    gchar *s_icon = NULL;
    gchar *s_category = NULL;

    ERL_NIF_TERM hints;
    ERL_NIF_TERM head;
    ERL_NIF_TERM tail;
    const ERL_NIF_TERM *array;
    int arity = 0;

    NotifyNotification *notify = NULL;

    ERL_NIF_TERM rv = atom_ok;


    if (!enif_inspect_iolist_as_binary(env, argv[0], &summary))
        return enif_make_badarg(env);

    if (!enif_inspect_iolist_as_binary(env, argv[1], &body))
        return enif_make_badarg(env);

    if (!enif_inspect_iolist_as_binary(env, argv[2], &icon))
        return enif_make_badarg(env);

    if (!enif_inspect_iolist_as_binary(env, argv[3], &category))
        return enif_make_badarg(env);

    if (!enif_get_int(env, argv[4], &urgency))
        return enif_make_badarg(env);

    if (!enif_get_int(env, argv[5], &timeout))
        return enif_make_badarg(env);

    if (!enif_is_list(env, argv[6]))
        return enif_make_badarg(env);

    hints = argv[6];

    s_summary = stralloc(&summary);
    s_body = stralloc(&body);
    s_icon = stralloc(&icon);
    s_category = stralloc(&category);

    if ( (s_summary == NULL) || (s_body == NULL) ||
            (s_icon == NULL) || (s_category == NULL)) {
        rv = enif_make_tuple2(env, atom_error, atom_nomem);
        goto ERR;
    }

    notify = notify_notification_new(s_summary, s_body, s_icon);

    notify_notification_set_category(notify, s_category);
    notify_notification_set_urgency(notify, urgency);
    notify_notification_set_timeout(notify, timeout);

    while (enif_get_list_cell(env, hints, &head, &tail)) {
        ERL_NIF_TERM key;
        ERL_NIF_TERM value = atom_undefined;

        if (enif_get_tuple(env, head, &arity, &array)) {
            switch (arity) {
                case 2:
                    value = array[1];
                    key = array[0];
                    break;
                default:
                    rv = enif_make_badarg(env);
                    goto ERR;
            }
        }
        else if (enif_is_atom(env, head)) {
            arity = 0;
            key = head;
        }
        else {
            rv = enif_make_badarg(env);
            goto ERR;
        }

        if (notify_hints_type(env, notify, arity, key, value) < 0) {
            rv = enif_make_badarg(env);
            goto ERR;
        }

        hints = tail;
    }

    notify_notification_show(notify, NULL);

ERR:
    if (notify)
        g_object_unref(G_OBJECT(notify));

    free(s_summary);
    free(s_body);
    free(s_icon);
    free(s_category);

    return rv;
}


    int
notify_hints_type(ErlNifEnv *env, NotifyNotification *notify, int arity, ERL_NIF_TERM key, ERL_NIF_TERM value)
{
    char a_key[256] = {0};

    union {
        ErlNifBinary b;
        int i;
        double d;
    } val;

    const ERL_NIF_TERM *byte = NULL;
    char a_byte[256] = {0};
    int len = 0;

    if (!enif_get_atom(env, key, a_key, sizeof(a_key), ERL_NIF_LATIN1))
        return -1;

    if (enif_get_int(env, value, &val.i))
        notify_notification_set_hint_int32(notify, a_key, val.i);
    else if (enif_get_double(env, value, &val.d))
        notify_notification_set_hint_double(notify, a_key, val.d);
    else if (enif_get_tuple(env, value, &len, &byte)) {
        if (len != 2 ||
                !enif_get_atom(env, byte[0], a_byte, sizeof(a_byte), ERL_NIF_LATIN1) ||
                (strcmp(a_byte, "byte") != 0) ||
                !enif_get_int(env, byte[1], &val.i))
            return -1;
        notify_notification_set_hint_byte(notify, a_key, (u_int8_t)val.i);
    }
    else if (arity == 0 || enif_inspect_iolist_as_binary(env, value, &val.b)) {
        gchar *tmpstr = NULL;

        if (arity > 0)
            tmpstr = stralloc(&val.b);

        notify_notification_set_hint_string(notify, a_key,
                (arity > 0 ? "" : tmpstr));

        free(tmpstr);
    }
    else
        return -1;

    return 0;
}


    static gchar *
stralloc(ErlNifBinary *bin)
{
    gchar *str = NULL;

    str = calloc(bin->size+1, sizeof(gchar));
    if (str == NULL)
        return NULL;

    (void)memcpy(str, bin->data, bin->size);
    return str;
}

static ErlNifFunc nif_funcs[] = {
    {"notify", 7, nif_notify}
};

ERL_NIF_INIT(notify, nif_funcs, load, NULL, NULL, unload)
