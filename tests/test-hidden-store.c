#include "osnx-hidden-store.h"

#include <glib/gstdio.h>

static void
test_hidden_ids_persist (void)
{
  g_autoptr (GError) error = NULL;
  g_autofree gchar *directory = g_dir_make_tmp ("osnotificationx-hidden-store-XXXXXX", &error);
  g_autofree gchar *path = NULL;
  OsnxHiddenStore *store;

  g_assert_no_error (error);
  g_assert_nonnull (directory);

  path = g_build_filename (directory, "state.ini", NULL);
  store = osnx_hidden_store_new (path);
  g_assert_false (osnx_hidden_store_contains (store, "first-id"));
  g_assert_true (osnx_hidden_store_add (store, "first-id"));
  g_assert_false (osnx_hidden_store_add (store, "first-id"));
  g_assert_true (osnx_hidden_store_save (store));
  osnx_hidden_store_free (store);

  store = osnx_hidden_store_new (path);
  g_assert_true (osnx_hidden_store_contains (store, "first-id"));
  g_assert_false (osnx_hidden_store_contains (store, "second-id"));
  osnx_hidden_store_free (store);

  g_assert_cmpint (g_remove (path), ==, 0);
  g_assert_cmpint (g_rmdir (directory), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/osnotificationx/hidden-store/persist", test_hidden_ids_persist);
  return g_test_run ();
}
