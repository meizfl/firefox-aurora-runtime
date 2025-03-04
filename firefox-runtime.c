#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>

#define DEFAULT_CONFIG_PATH "/etc/firefox-aurora/firefox-runtime.toml"
#define DEFAULT_MOZ_PATH "/opt/firefox-dev/firefox"
#define DEFAULT_MOZ_NATIVE_MSG_PATH "/usr/lib64/mozilla/native-messaging-hosts/"
#define HOME_ENV "HOME"
#define MAX_LINE_LENGTH 512

typedef struct {
    char moz_path[512];
    char native_messaging_path[512];
    bool wayland_enable;
    bool enable_kde_integration;
    bool enable_gnome_integration;
} ConfigParams;

bool file_exists(const char *path) {
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

void create_dir_if_not_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

void create_symlink(const char *src, const char *dest) {
    if (file_exists(src) && !file_exists(dest)) {
        symlink(src, dest);
    }
}

char* trim_string(char* str) {
    if (!str) return NULL;

    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) // All spaces?
        return str;

    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    *(end+1) = 0;

    return str;
}

char* remove_quotes(char* str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    if (len >= 2 && (
        (str[0] == '"' && str[len-1] == '"') ||
        (str[0] == '\'' && str[len-1] == '\''))) {
        str[len-1] = '\0';
    return str + 1;
        }

        return str;
}

bool parse_boolean(const char* value) {
    if (!value) return false;

    char* cleaned = strdup(value);
    cleaned = trim_string(cleaned);

    bool result = false;
    if (strcmp(cleaned, "true") == 0 || strcmp(cleaned, "yes") == 0 || strcmp(cleaned, "1") == 0) {
        result = true;
    }

    free(cleaned);
    return result;
}

bool parse_config(const char *config_path, ConfigParams *params) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char current_section[64] = "";

    strncpy(params->moz_path, DEFAULT_MOZ_PATH, sizeof(params->moz_path));
    strncpy(params->native_messaging_path, DEFAULT_MOZ_NATIVE_MSG_PATH, sizeof(params->native_messaging_path));
    params->wayland_enable = true;
    params->enable_kde_integration = true;
    params->enable_gnome_integration = true;

    fp = fopen(config_path, "r");
    if (!fp) {
        fprintf(stderr, "Warning: Cannot open config file %s. Using default values.\n", config_path);
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        char* trimmed = trim_string(line);
        if (trimmed[0] == '#' || trimmed[0] == '\0' || trimmed[0] == '\n') {
            continue;
        }

        if (trimmed[0] == '[') {
            char* end = strchr(trimmed, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, trimmed + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
            continue;
        }

        char* eq = strchr(trimmed, '=');
        if (eq) {
            *eq = '\0';
            char* key = trim_string(trimmed);
            char* value = trim_string(eq + 1);
            value = remove_quotes(value);

            if (strcmp(current_section, "general") == 0) {
                if (strcmp(key, "moz_path") == 0) {
                    strncpy(params->moz_path, value, sizeof(params->moz_path) - 1);
                    params->moz_path[sizeof(params->moz_path) - 1] = '\0';
                } else if (strcmp(key, "native_messaging_path") == 0) {
                    strncpy(params->native_messaging_path, value, sizeof(params->native_messaging_path) - 1);
                    params->native_messaging_path[sizeof(params->native_messaging_path) - 1] = '\0';
                }
            } else if (strcmp(current_section, "wayland") == 0) {
                if (strcmp(key, "enable") == 0) {
                    params->wayland_enable = parse_boolean(value);
                }
            } else if (strcmp(current_section, "desktop") == 0) {
                if (strcmp(key, "enable_kde_integration") == 0) {
                    params->enable_kde_integration = parse_boolean(value);
                } else if (strcmp(key, "enable_gnome_integration") == 0) {
                    params->enable_gnome_integration = parse_boolean(value);
                }
            }
        }
    }

    fclose(fp);
    return true;
}

void setup_kde_integration(const char *home, const char *native_msg_path) {
    char src_path[512];
    char dest_path[512];
    char mozilla_dir[512];
    char native_hosts_dir[512];

    snprintf(mozilla_dir, sizeof(mozilla_dir), "%s/.mozilla", home);
    snprintf(native_hosts_dir, sizeof(native_hosts_dir), "%s/native-messaging-hosts", mozilla_dir);
    snprintf(src_path, sizeof(src_path), "%sorg.kde.plasma.browser_integration.json", native_msg_path);
    snprintf(dest_path, sizeof(dest_path), "%s/org.kde.plasma.browser_integration.json", native_hosts_dir);

    if (!file_exists(dest_path)) {
        create_dir_if_not_exists(mozilla_dir);
        create_dir_if_not_exists(native_hosts_dir);
        create_symlink(src_path, dest_path);
    }
}

void setup_gnome_integration(const char *home, const char *native_msg_path) {
    char src_path1[512], src_path2[512];
    char dest_path1[512], dest_path2[512];
    char mozilla_dir[512];
    char native_hosts_dir[512];

    snprintf(mozilla_dir, sizeof(mozilla_dir), "%s/.mozilla", home);
    snprintf(native_hosts_dir, sizeof(native_hosts_dir), "%s/native-messaging-hosts", mozilla_dir);

    snprintf(src_path1, sizeof(src_path1), "%sorg.gnome.browser_connector.json", native_msg_path);
    snprintf(src_path2, sizeof(src_path2), "%sorg.gnome.chrome_gnome_shell.json", native_msg_path);

    snprintf(dest_path1, sizeof(dest_path1), "%s/org.gnome.browser_connector.json", native_hosts_dir);
    snprintf(dest_path2, sizeof(dest_path2), "%s/org.gnome.chrome_gnome_shell.json", native_hosts_dir);

    create_dir_if_not_exists(mozilla_dir);
    create_dir_if_not_exists(native_hosts_dir);

    create_symlink(src_path1, dest_path1);
    create_symlink(src_path2, dest_path2);
}

int main(int argc, char *argv[]) {
    const char *config_path = getenv("FIREFOX_RUNTIME_AURORA");
    if (!config_path) {
        config_path = DEFAULT_CONFIG_PATH;
    }

    ConfigParams config;
    parse_config(config_path, &config);

    setenv("FIREFOX_RUNTIME_AURORA", config_path, 1);

    const char *xdg_desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *xdg_session = getenv("XDG_SESSION_TYPE");
    const char *wayland_display = getenv("WAYLAND_DISPLAY");
    const char *moz_disable_wayland = getenv("MOZ_DISABLE_WAYLAND");
    const char *home = getenv(HOME_ENV);

    if (config.wayland_enable && !moz_disable_wayland && xdg_desktop &&
        ((strcmp(xdg_desktop, "GNOME") == 0 && wayland_display) ||
        (xdg_session && strcmp(xdg_session, "wayland") == 0))) {
        setenv("MOZ_ENABLE_WAYLAND", "1", 1);
    setenv("MOZ_DBUS_REMOTE", "1", 1);
        }

        if (config.enable_kde_integration && xdg_desktop &&
            strcmp(xdg_desktop, "KDE") == 0 && home) {
            setup_kde_integration(home, config.native_messaging_path);
            }

            if (config.enable_gnome_integration && xdg_desktop &&
                strcmp(xdg_desktop, "GNOME") == 0 && home) {
                setup_gnome_integration(home, config.native_messaging_path);
                }

                setenv("MOZ_APP_LAUNCHER", argv[0], 1);

            execv(config.moz_path, argv);

            perror("execv failed");
            return 1;
}
