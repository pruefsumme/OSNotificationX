#ifndef OSNX_HIDDEN_STORE_H
#define OSNX_HIDDEN_STORE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _OsnxHiddenStore OsnxHiddenStore;

OsnxHiddenStore *osnx_hidden_store_new (const gchar *path);
void osnx_hidden_store_free (OsnxHiddenStore *store);
gboolean osnx_hidden_store_contains (OsnxHiddenStore *store, const gchar *id);
gboolean osnx_hidden_store_add (OsnxHiddenStore *store, const gchar *id);
gboolean osnx_hidden_store_save (OsnxHiddenStore *store);

G_END_DECLS

#endif
