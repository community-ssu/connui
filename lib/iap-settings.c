#include <dbus/dbus.h>
#include <gconf/gconf-client.h>

#include <string.h>
#include <libintl.h>

#include "connui-log.h"
#include "iap-common.h"
#include "iap-settings.h"

gchar *
iap_settings_create_iap_id()
{
  GConfClient *gconf;
  GError *error = NULL;
  gchar *uuid = NULL;

  if (!(gconf = gconf_client_get_default()))
  {
    CONNUI_ERR("Unable to get GConfClient!");
    return NULL;
  }

  while (1)
  {
    gboolean exists;
    gchar *s;
    gchar *client_dir;

    g_free(uuid);

    if (!g_file_get_contents("/proc/sys/kernel/random/uuid", &uuid, NULL,
                             &error) || !uuid)
    {
      if (error)
      {
        CONNUI_ERR("Unable to read file: %s", error->message);
        g_error_free(error);
      }
      else
        CONNUI_ERR("Unable to read file: no error");

      return NULL;
    }

    g_strchomp(g_strchug(uuid));

    s = gconf_escape_key(uuid, -1);
    client_dir = g_strconcat("/system/osso/connectivity/IAP", "/", s, NULL);
    g_free(s);
    exists = gconf_client_dir_exists(gconf, client_dir, NULL);
    g_free(client_dir);

    if (!exists)
    {
      g_object_unref(gconf);
      return uuid;
    }
  }

  return NULL;
}

const char *
iap_settings_enum_to_gconf_type(iap_type type)
{
  switch (type)
  {
    case DUN_CDMA_PSD:
      return "DUN_CDMA_PSD";
    case DUN_GSM_PS:
      return "DUN_GSM_PS";
    case DUN_CDMA_CSD:
      return "DUN_CDMA_CSD";
    case DUN_CDMA_QNC:
      return "DUN_CDMA_QNC";
    case DUN_GSM_CS:
      return "DUN_GSM_CS";
    case WLAN_INFRA:
      return "WLAN_INFRA";
    case WIMAX:
      return "WIMAX";
    case WLAN_ADHOC:
      return "WLAN_ADHOC";
    default:
      break;
  }

  return NULL;
}

iap_type
iap_settings_gconf_type_to_enum(const char *type)
{
  if (!type || !*type)
    return INVALID;

  if (!strcmp("DUN_GSM_CS", type))
    return DUN_GSM_CS;

  if (!strcmp("DUN_GSM_PS", type))
    return DUN_GSM_PS;

  if (!strcmp("DUN_CDMA_CSD", type))
    return DUN_CDMA_CSD;

  if (!strcmp("DUN_CDMA_QNC", type))
    return DUN_CDMA_QNC;

  if (!strcmp("DUN_CDMA_PSD", type))
    return DUN_CDMA_PSD;

  if (!strcmp("WLAN_ADHOC", type))
    return WLAN_ADHOC;

  if (!strcmp("WLAN_INFRA", type))
    return WLAN_INFRA;

  if (!strcmp("WIMAX", type))
    return WIMAX;

  return UNKNOWN;
}

gchar *
iap_settings_get_auto_connect()
{
  GConfClient *gconf;
  gchar *auto_connect;

  gconf = gconf_client_get_default();
  auto_connect =
      gconf_client_get_string(gconf,
                              "/system/osso/connectivity/IAP/auto_connect",
                              NULL);
  g_object_unref(gconf);

  return auto_connect;
}

gboolean
iap_settings_iap_is_easywlan(const gchar *iap_name)
{
  if (!iap_name)
    return FALSE;

  return !strncmp(iap_name, "[Easy", 5);
}

gboolean
iap_settings_remove_iap(const gchar *iap_name)
{
  GConfClient *gconf;
  gchar *s;
  gchar *key;

  if (!iap_name)
    return FALSE;

  gconf = gconf_client_get_default();

  if (!gconf)
    return FALSE;

  s = gconf_escape_key(iap_name, -1);
  key = g_strconcat("/system/osso/connectivity/IAP", "/", s, NULL);
  gconf_client_recursive_unset(
        gconf, key, GCONF_UNSET_INCLUDING_SCHEMA_NAMES, NULL);
  gconf_client_suggest_sync(gconf, NULL);
  g_free(key);
  g_free(s);
  g_object_unref(gconf);

  return TRUE;
}

gboolean
iap_settings_is_iaptype_supported(const gchar *type)
{
  GConfClient *gconf = gconf_client_get_default();
  gchar *dir;
  gboolean supported;
  GError *error = NULL;

  if (!gconf)
  {
    CONNUI_ERR("Unable to get GConf");
    return FALSE;
  }

  dir = g_strdup_printf(
        "%s/%s", "/system/osso/connectivity/network_type", type);
  supported = gconf_client_dir_exists(gconf, dir, &error);

  if (error)
  {
    CONNUI_ERR("%s", error->message);
    g_error_free(error);
  }

  g_free(dir);
  g_object_unref(gconf);

  return supported;
}

static gboolean
is_mobile_internet(const gchar *iap)
{
  const gchar *p;

  if (!iap)
    return FALSE;

  for (p = strchr(iap, '@'); p; p = strchr(p + 1, '@'))
  {
    if (strlen(p) < 4)
      break;

    if (p[3] == '@'&& g_ascii_isdigit(p[1]) && g_ascii_isdigit(p[2]))
        return TRUE;
  }

  return FALSE;
}

GConfValue *
iap_settings_get_gconf_value(const gchar *iap, const gchar *key)
{
  GConfClient *gconf_client;
  gchar *s;
  gchar *iap_gconf_key;
  GConfValue *value;
  GError *error = NULL;

  if (!iap || !key)
  {
    CONNUI_ERR("%s not specified", iap ? "key" : iap);
    return NULL;
  }

  if (!*iap)
  {
    CONNUI_ERR("IAP ID is empty string");
    return NULL;
  }

  gconf_client = gconf_client_get_default();

  if (!gconf_client)
  {
    CONNUI_ERR("Unable to get GConfClient");
    return NULL;
  }

  if (is_mobile_internet(iap))
  {
    iap_gconf_key = g_strdup_printf("/system/osso/connectivity/IAP/%s/%s", iap,
                                    key);

    if (gconf_client_dir_exists(gconf_client, iap_gconf_key, NULL))
    {
      if (iap_gconf_key)
        goto read_value;
    }
    else
      g_free(iap_gconf_key);
  }

  s = gconf_escape_key(iap, -1);
  iap_gconf_key = g_strdup_printf("/system/osso/connectivity/IAP/%s/%s", s,
                                  key);
  g_free(s);

read_value:
  value = gconf_client_get(gconf_client, iap_gconf_key, &error);
  g_free(iap_gconf_key);

  if (error)
  {
    CONNUI_ERR("could not read key %s for iap %s: '%s'", key, iap,
               error->message);
    g_clear_error(&error);
    value = NULL;
  }

  g_object_unref(gconf_client);

  return value;
}

gchar *
iap_settings_get_name(const gchar *iap)
{
  GConfValue *val;

  if (!iap || !*iap)
    return NULL;

  val = iap_settings_get_gconf_value(iap, "service_type");

  if (val)
  {
    gchar *service_type = g_strdup(gconf_value_get_string(val));

    gconf_value_free(val);
    val = iap_settings_get_gconf_value(iap, "service_id");

    if (val)
    {
      gchar *s = NULL;
      gchar *domainname = NULL;
      gchar *msgid = NULL;
      gchar *service_id = g_strdup(gconf_value_get_string(val));

      gconf_value_free(val);
      iap_common_get_service_properties(service_type, service_id,
                                        "gettext_catalog", &domainname,
                                        "name", &msgid,
                                        NULL);
      if (msgid && domainname)
        s = g_strdup(dgettext(domainname, msgid));

      g_free(msgid);
      g_free(domainname);
      g_free(service_type);
      g_free(service_id);

      if (s)
        return s;
    }

    g_free(service_type);
  }

  val = iap_settings_get_gconf_value(iap, "name");

  if (val)
  {

    gchar *s = g_strdup(gconf_value_get_string(val));
    gconf_value_free(val);

    return s;
  }

  val = iap_settings_get_gconf_value(iap, "temporary");

  if (val)
  {
    if (gconf_value_get_bool(val) == 1)
    {
      gchar *s = iap_settings_get_wlan_ssid(iap);

      gconf_value_free(val);

      if (s)
      {
        wlan_common_mangle_ssid(s, strlen(s));
        return s;
      }
    }
    else
      gconf_value_free(val);
  }

  val = iap_settings_get_gconf_value(iap, "type");

  if (val)
  {
    gchar *s = get_iap_name_by_type(gconf_value_get_string(val));

    gconf_value_free(val);

    if (s)
      return s;
  }

  return g_strdup(iap);
}