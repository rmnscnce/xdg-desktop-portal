#include <config.h>

#include "account.h"

#include <libportal/portal.h>
#include "src/xdp-utils.h"
#include "src/xdp-impl-dbus.h"

extern char outdir[];

static gboolean got_info = FALSE;

static void
open_file_cb (GObject *obj,
                 GAsyncResult *result,
                 gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (obj);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;
  GKeyFile *keyfile = data;
  int response;
  int domain;
  int code;

  response = g_key_file_get_integer (keyfile, "result", "response", NULL);
  domain = g_key_file_get_integer (keyfile, "result", "error_domain", NULL);
  code = g_key_file_get_integer (keyfile, "result", "error_code", NULL);

  ret = xdp_portal_open_file_finish (portal, result, &error);
  if (response == 0)
    {
      const char * const *uris;
      g_auto(GStrv) expected_uris = NULL;
      g_autofree char *expected_choices = NULL;
      g_autoptr(GVariant) choices = NULL;

      g_assert_no_error (error);
      g_variant_lookup (ret, "uris", "^a&s", &uris);
      expected_uris = g_key_file_get_string_list (keyfile, "result", "uris", NULL, NULL);

      g_assert (g_strv_equal (uris, (const char * const *)expected_uris)); 

      expected_choices = g_key_file_get_string (keyfile, "result", "choices", NULL);
      g_variant_lookup (ret, "choices", "@a(ss)", &choices);
      if (expected_choices)
        {
          g_autoptr(GVariant) c = NULL;
          g_assert_nonnull (choices);
          c = g_variant_parse (G_VARIANT_TYPE ("a(ss)"), expected_choices, NULL, NULL, NULL);
          g_assert_true (g_variant_equal (choices, c));
	}
      else
        {
          g_assert_null (choices);
        }
    }
  else if (response == 1)
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  else if (response == 2)
    g_assert_error (error, domain, code);
  else
    g_assert_not_reached ();

  got_info = TRUE;

  g_main_context_wakeup (NULL);
}

void
test_open_file_libportal (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 0);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_delay (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_cancel (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 1);
  g_key_file_set_integer (keyfile, "result", "response", 1);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

static gboolean
cancel_call (gpointer data)
{
  GCancellable *cancellable = data;

  g_debug ("cancel call");
  g_cancellable_cancel (cancellable);

  return G_SOURCE_REMOVE;
}

void
test_open_file_close (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "expect-close", 1);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 1);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  cancellable = g_cancellable_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, NULL, cancellable, open_file_cb, keyfile);

  g_timeout_add (100, cancel_call, cancellable);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_multiple (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    "file:///test/file2",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", TRUE, TRUE, NULL, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_filters1 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) filters = NULL;
  const char *filter_string = 
    "[('Images', [(0, '*ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]";

  filters = g_variant_parse (G_VARIANT_TYPE ("a(sa(us))"), filter_string, NULL, NULL, &error);

  keyfile = g_key_file_new ();

  g_assert_no_error (error);

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "filters", filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, filters, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_filters2 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) filters = NULL;
  const char *filter_string = 
    "[('Images', [(0, '*ico'), (1, 'image/png')]), ('Text', [(4, '*.txt')])]"; /* invalid type */

  filters = g_variant_parse (G_VARIANT_TYPE ("a(sa(us))"), filter_string, NULL, NULL, &error);

  keyfile = g_key_file_new ();

  g_assert_no_error (error);

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "filters", filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_INVALID_ARGUMENT);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, filters, NULL, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_current_filter1 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) filters = NULL;
  g_autoptr(GVariant) current_filter = NULL;
  const char *filter_string = "[('Images', [(0, '*ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]";
  const char *current_filter_string = "('Text', [(0, '*.txt')])";

  filters = g_variant_parse (G_VARIANT_TYPE ("a(sa(us))"), filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  current_filter = g_variant_parse (G_VARIANT_TYPE ("(sa(us))"), current_filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "filters", filter_string);
  g_key_file_set_string (keyfile, "backend", "current_filter", current_filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, filters, current_filter, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_current_filter2 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) current_filter = NULL;
  const char *current_filter_string = "('Text', [(0, '*.txt')])";

  current_filter = g_variant_parse (G_VARIANT_TYPE ("(sa(us))"), current_filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "current_filter", current_filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, current_filter, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_current_filter3 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) current_filter = NULL;
  const char *current_filter_string = "('Text', [(6, '*.txt')])"; /* invalid type */

  current_filter = g_variant_parse (G_VARIANT_TYPE ("(sa(us))"), current_filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "current_filter", current_filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_INVALID_ARGUMENT);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, current_filter, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_current_filter4 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) filters = NULL;
  g_autoptr(GVariant) current_filter = NULL;
  const char *filter_string = "[('Images', [(0, '*ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]";
  const char *current_filter_string = "('Something else', [(0, '*.sth.else')])"; /* not in the list */

  filters = g_variant_parse (G_VARIANT_TYPE ("a(sa(us))"), filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  current_filter = g_variant_parse (G_VARIANT_TYPE ("(sa(us))"), current_filter_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "filters", filter_string);
  g_key_file_set_string (keyfile, "backend", "current_filter", current_filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_INVALID_ARGUMENT);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, filters, current_filter, NULL, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_choices1 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) choices = NULL;
  const char *choices_string = "[('encoding', 'Encoding', [('utf8', 'Unicode'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false'), ('third', 'Third', [('a', 'A'), ('b', 'B')], '')]";
  const char *chosen_string = "[('encoding', 'utf8'), ('reencode', 'true'), ('third', 'a')]";

  choices = g_variant_parse (G_VARIANT_TYPE ("a(ssa(ss)s)"), choices_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "choices", choices_string);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string (keyfile, "result", "choices", chosen_string);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, choices, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_choices2 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GVariant) choices = NULL;
  const char *choices_string = "[('encoding', 'Encoding', [('utf8', ''), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]"; /* invalid: empty label */

  choices = g_variant_parse (G_VARIANT_TYPE ("a(ssa(ss)s)"), choices_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "choices", choices_string);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_INVALID_ARGUMENT);

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, choices, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_open_file_choices3 (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  g_autoptr(GVariant) choices = NULL;
  const char *choices_string = "[('', 'Encoding', [('utf8', 'Unicode'), ('latin15', 'Western')], 'latin15'), ('reencode', 'Reencode', [], 'false')]"; /* invalid: empty id */

  choices = g_variant_parse (G_VARIANT_TYPE ("a(ssa(ss)s)"), choices_string, NULL, NULL, &error);
  g_assert_no_error (error);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "choices", choices_string);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_INVALID_ARGUMENT);

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_open_file (portal, NULL, "test", FALSE, FALSE, NULL, NULL, choices, NULL, open_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

/* tests of SaveFile below */

static void
save_file_cb (GObject *obj,
              GAsyncResult *result,
              gpointer data)
{
  XdpPortal *portal = XDP_PORTAL (obj);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;
  GKeyFile *keyfile = data;
  int response;
  int domain;
  int code;

  response = g_key_file_get_integer (keyfile, "result", "response", NULL);
  domain = g_key_file_get_integer (keyfile, "result", "error_domain", NULL);
  code = g_key_file_get_integer (keyfile, "result", "error_code", NULL);

  ret = xdp_portal_save_file_finish (portal, result, &error);
  if (response == 0)
    {
      const char * const *uris;
      g_auto(GStrv) expected = NULL;

      g_assert_no_error (error);
      g_variant_lookup (ret, "uris", "^a&s", &uris);
      expected = g_key_file_get_string_list (keyfile, "result", "uris", NULL, NULL);

      g_assert (g_strv_equal (uris, (const char * const *)expected)); 
    }
  else if (response == 1)
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  else if (response == 2)
    g_assert_error (error, domain, code);
  else
    g_assert_not_reached ();

  got_info = TRUE;

  g_main_context_wakeup (NULL);
}

void
test_save_file_libportal (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 0);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, NULL, NULL, NULL, NULL, save_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_save_file_delay (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, NULL, NULL, NULL, NULL, save_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_save_file_cancel (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 1);
  g_key_file_set_integer (keyfile, "result", "response", 1);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, NULL, NULL, NULL, NULL, save_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_save_file_close (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GCancellable) cancellable = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "expect-close", 1);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 1);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  cancellable = g_cancellable_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, NULL, NULL, NULL, cancellable, save_file_cb, keyfile);

  g_timeout_add (100, cancel_call, cancellable);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

void
test_save_file_filters (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file1",
    NULL
  };
  g_autoptr(GVariant) filters = NULL;
  const char *filter_string = 
    "[('Images', [(0, '*ico'), (1, 'image/png')]), ('Text', [(0, '*.txt')])]";

  filters = g_variant_parse (G_VARIANT_TYPE ("a(sa(us))"), filter_string, NULL, NULL, &error);

  keyfile = g_key_file_new ();

  g_assert_no_error (error);

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_string (keyfile, "backend", "filters", filter_string);
  g_key_file_set_integer (keyfile, "result", "response", 0);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, filters, NULL, NULL, NULL, save_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);
}

#define BACKEND_BUS_NAME "org.freedesktop.impl.portal.Test"
#define BACKEND_OBJECT_PATH "/org/freedesktop/portal/desktop"

void
test_save_file_lockdown (void)
{
  g_autoptr(XdpPortal) portal = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *path = NULL;
  const char * uris[] = {
    "file:///test/file",
    NULL
  };
  g_autoptr(GDBusConnection) session_bus = NULL;
  g_autoptr(GDBusProxy) lockdown = NULL;

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);

  lockdown = G_DBUS_PROXY (xdp_impl_lockdown_proxy_new_sync (session_bus,
			                                     0,
                                                             BACKEND_BUS_NAME,
                                                             BACKEND_OBJECT_PATH,
                                                             NULL,
							     &error));
  g_assert_no_error (error);

  xdp_impl_lockdown_set_disable_save_to_disk (XDP_IMPL_LOCKDOWN (lockdown), TRUE);

  keyfile = g_key_file_new ();

  g_key_file_set_integer (keyfile, "backend", "delay", 200);
  g_key_file_set_integer (keyfile, "backend", "response", 0);
  g_key_file_set_integer (keyfile, "result", "response", 2);
  g_key_file_set_integer (keyfile, "result", "error_domain", XDG_DESKTOP_PORTAL_ERROR);
  g_key_file_set_integer (keyfile, "result", "error_code", XDG_DESKTOP_PORTAL_ERROR_NOT_ALLOWED);
  g_key_file_set_string_list (keyfile, "result", "uris", uris, g_strv_length ((char **)uris));

  path = g_build_filename (outdir, "filechooser", NULL);
  g_key_file_save_to_file (keyfile, path, &error);
  g_assert_no_error (error);

  portal = xdp_portal_new ();

  got_info = FALSE;
  xdp_portal_save_file (portal, NULL, "test", FALSE, "test_file.txt", NULL, NULL, NULL, NULL, NULL, NULL, save_file_cb, keyfile);

  while (!got_info)
    g_main_context_iteration (NULL, TRUE);

  xdp_impl_lockdown_set_disable_save_to_disk (XDP_IMPL_LOCKDOWN (lockdown), FALSE);
}

