#include "osnx-log-entry.h"

static GVariant *
make_entry_variant (const gchar *id,
                    const gchar *app_id,
                    const gchar *app_name,
                    gboolean     is_read)
{
  GVariantBuilder actions;

  g_variant_builder_init (&actions, G_VARIANT_TYPE ("a(ss)"));

  return g_variant_ref_sink (g_variant_new ("(sxssssss@a(ss)ib)",
                                           id,
                                           (gint64) 1710000000,
                                           "UTC",
                                           app_id,
                                           app_name,
                                           "dialog-information",
                                           "Summary",
                                           "Body",
                                           g_variant_builder_end (&actions),
                                           5000,
                                           is_read));
}

static void
test_parse_entry (void)
{
  g_autoptr (GVariant) variant = make_entry_variant ("entry-1", "org.example.App", "Example", FALSE);
  g_autoptr (GError) error = NULL;
  OsnxLogEntry *entry = osnx_log_entry_new_from_variant (variant, &error);
  g_autofree gchar *key = NULL;

  g_assert_no_error (error);
  g_assert_nonnull (entry);
  g_assert_cmpstr (entry->id, ==, "entry-1");
  g_assert_cmpstr (entry->app_id, ==, "org.example.App");
  g_assert_cmpstr (entry->app_name, ==, "Example");
  g_assert_false (entry->is_read);

  key = osnx_log_entry_app_key (entry);
  g_assert_cmpstr (key, ==, "org.example.App");

  osnx_log_entry_free (entry);
}

static void
test_parse_list_result (void)
{
  GVariantBuilder entries;
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) parsed = NULL;

  g_variant_builder_init (&entries, G_VARIANT_TYPE ("a" OSNX_LOG_ENTRY_VARIANT_TYPE));
  g_variant_builder_add_value (&entries, make_entry_variant ("entry-1", "org.example.App", "Example", FALSE));
  g_variant_builder_add_value (&entries, make_entry_variant ("entry-2", "org.example.App", "Example", TRUE));

  result = g_variant_ref_sink (g_variant_new ("(@a" OSNX_LOG_ENTRY_VARIANT_TYPE ")",
                                             g_variant_builder_end (&entries)));

  parsed = osnx_log_entries_new_from_list_result (result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (parsed);
  g_assert_cmpuint (parsed->len, ==, 2);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/osnotificationx/log-entry/parse-entry", test_parse_entry);
  g_test_add_func ("/osnotificationx/log-entry/parse-list-result", test_parse_list_result);
  return g_test_run ();
}
