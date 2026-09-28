/* Nuke Wireless RootHide adapter for the original app's private runtime.
 * The bundle and private runtime identifiers below are compatibility hooks.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#if __has_include(<net/route.h>)
#include <net/route.h>
#else
/* iPhoneOS SDKs omit the BSD routing-table declarations. Keep the Darwin
 * rt_msghdr layout needed by the ARP cache parser local to this tweak. */
#define NET_RT_FLAGS 2
#define RTF_LLINFO 0x400
#define RTAX_DST 0
#define RTAX_GATEWAY 1
#define RTAX_MAX 8
struct rt_metrics {
    unsigned long rmx_locks, rmx_mtu, rmx_hopcount, rmx_expire;
    unsigned long rmx_recvpipe, rmx_sendpipe, rmx_ssthresh, rmx_rtt;
    unsigned long rmx_rttvar, rmx_weight;
    uint32_t rmx_filler[3];
};
struct rt_msghdr {
    unsigned short rtm_msglen;
    unsigned char rtm_version, rtm_type;
    unsigned short rtm_index;
    int rtm_flags, rtm_addrs;
    pid_t rtm_pid;
    int rtm_seq, rtm_errno, rtm_use;
    uint32_t rtm_inits;
    struct rt_metrics rtm_rmx;
};
#endif
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>

typedef void *id;
typedef void *Class;
typedef void *SEL;
typedef void *Method;
typedef void *IMP;
typedef unsigned long NSUInteger;
typedef signed char BOOL;

extern Class objc_getClass(const char *name);
extern Class object_getClass(id object);
extern const char *class_getName(Class cls);
extern SEL sel_registerName(const char *name);
extern Method class_getInstanceMethod(Class cls, SEL name);
extern Method class_getClassMethod(Class cls, SEL name);
extern IMP method_setImplementation(Method method, IMP imp);
extern id objc_msgSend(id receiver, SEL selector, ...);
extern void *popen(const char *, const char *);
extern char *fgets(char *, int, void *);
extern int pclose(void *);
extern int snprintf(char *, unsigned long, const char *, ...);
extern int open(const char *, int, ...);
extern long write(int, const void *, unsigned long);
extern int close(int);
extern void *dlopen(const char *, int);
extern void *dlsym(void *, const char *);
extern char *dlerror(void);
extern void CFRelease(void *);
extern void *dispatch_queue_create(const char *label, void *attr);
extern void dispatch_async_f(void *queue, void *context, void (*work)(void *));
extern void *dispatch_get_main_queue(void);
extern Class objc_allocateClassPair(Class superclass, const char *name, unsigned long extraBytes);
extern void objc_registerClassPair(Class cls);
extern BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types);

static BOOL (*original_exists)(id, SEL, id);
static void (*original_launch_path)(id, SEL, id);
static void (*original_arguments)(id, SEL, id);
static void (*original_terminate)(id, SEL);
static void (*original_interrupt)(id, SEL);
static void (*original_launch)(id, SEL);
static void (*original_swift_unblock)(uint64_t, uint64_t);
static id (*swift_string_to_nsstring)(uint64_t, uint64_t);
static void (*original_did_appear)(id, SEL, BOOL);
static void (*original_tab_item_set_title)(id, SEL, id);
static void (*original_navigation_item_set_title)(id, SEL, id);
static void (*original_controller_set_title)(id, SEL, id);
static void (*original_found_device)(id, SEL, id);
static id (*original_scanner_init)(id, SEL, id, BOOL);
static void (*original_scanner_start)(id, SEL);
static void (*original_scanner_finished)(id, SEL, NSUInteger);
static void (*original_scanner_failed)(id, SEL);
static id oui_brands;
static int ui_dump_count;
struct cg_point { double x, y; };
struct cg_size { double width, height; };
struct cg_rect { struct cg_point origin; struct cg_size size; };
struct ui_edge_insets { double top, left, bottom, right; };

struct active_block {
    char ip[32];
    char real_mac[32];
    int pid;
};
static struct active_block blocks[64];
static struct active_block pending_block;
static int repair_in_progress;
static int capture_root_task;
static id captured_root_task;
struct scanned_device {
    char ip[32];
    char mac[32];
    char name[96];
};
static struct scanned_device scanned_devices[64];
static int scanned_count;
static struct scanned_device confirmed_devices[64];
static int confirmed_count;
static char bulk_ips[64][32];
static int bulk_count;
static id bulk_button;
static id bulk_panel;
static id status_label;
static id alias_button;
static id refresh_control;
static id wifi_scanner;
static id bulk_target;
static int bulk_confirm_count;
static time_t bulk_confirm_until;
static int bulk_confirm_pending;
static int scan_in_progress;
static time_t scan_finished_at;
static time_t scan_requested_at;
static int scan_timed_out;
static int bulk_last_failed;
static id presenting_controller;
static id info_presenting_controller;
static id info_root_view;
static id info_target;
static id alias_alert;
static char alias_selected_mac[32];
static char alias_selected_ip[32];
static char name_lookup_attempted[64][32];
static int name_lookup_attempt_count;
static void *name_lookup_queue;

#define NUKE_WIRELESS_RELEASE_VERSION "1.0.48-dev"

static int starts_with(const char *value, const char *prefix) {
    if (!value) return 0;
    while (*prefix) {
        if (*value++ != *prefix++) return 0;
    }
    return 1;
}

static int contains(const char *value, const char *needle) {
    if (!value || !needle) return 0;
    for (; *value; ++value) if (starts_with(value, needle)) return 1;
    return 0;
}

static char ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

static int contains_case_insensitive(const char *value, const char *needle) {
    if (!value || !needle || !*needle) return 0;
    for (; *value; ++value) {
        const char *left = value;
        const char *right = needle;
        while (*left && *right && ascii_lower(*left) == ascii_lower(*right)) {
            ++left;
            ++right;
        }
        if (!*right) return 1;
    }
    return 0;
}

static int equals(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static const char *utf8(id string) {
    return string ? ((const char *(*)(id, SEL))objc_msgSend)(
        string, sel_registerName("UTF8String")) : 0;
}

static id jailbreak_prefix(void);
static id string_from_utf8(const char *value);
static void update_dashboard(void);
static int request_wifi_scan(void);
static void scan_finished_on_main(void *context);
static id add_info_label(id parent, struct cg_rect frame, const char *text,
                         double size, double weight, id color,
                         long alignment, long lines);
static void render_info_screen(id root);

enum nw_language {
    NW_LANG_ES = 0,
    NW_LANG_EN,
    NW_LANG_FR,
    NW_LANG_DE,
    NW_LANG_ZH_HANS,
    NW_LANG_ZH_HANT,
    NW_LANG_JA,
};

static enum nw_language cached_language = NW_LANG_ES;
static int cached_language_initialized;

static enum nw_language current_language(void) {
    if (cached_language_initialized) return cached_language;
    Class defaults_class = objc_getClass("NSUserDefaults");
    id defaults = ((id (*)(id, SEL))objc_msgSend)(
        defaults_class, sel_registerName("standardUserDefaults"));
    id value = ((id (*)(id, SEL, id))objc_msgSend)(
        defaults, sel_registerName("stringForKey:"),
        string_from_utf8("NukeWirelessLanguage"));
    const char *code = utf8(value);
    if (equals(code, "en")) cached_language = NW_LANG_EN;
    else if (equals(code, "fr")) cached_language = NW_LANG_FR;
    else if (equals(code, "de")) cached_language = NW_LANG_DE;
    else if (equals(code, "zh-Hans")) cached_language = NW_LANG_ZH_HANS;
    else if (equals(code, "zh-Hant")) cached_language = NW_LANG_ZH_HANT;
    else if (equals(code, "ja")) cached_language = NW_LANG_JA;
    else cached_language = NW_LANG_ES;
    cached_language_initialized = 1;
    return cached_language;
}

static const char *language_code(enum nw_language language) {
    switch (language) {
        case NW_LANG_EN: return "en";
        case NW_LANG_FR: return "fr";
        case NW_LANG_DE: return "de";
        case NW_LANG_ZH_HANS: return "zh-Hans";
        case NW_LANG_ZH_HANT: return "zh-Hant";
        case NW_LANG_JA: return "ja";
        default: return "es";
    }
}

static const char *language_name(enum nw_language language) {
    switch (language) {
        case NW_LANG_EN: return "English";
        case NW_LANG_FR: return "Français";
        case NW_LANG_DE: return "Deutsch";
        case NW_LANG_ZH_HANS: return "简体中文";
        case NW_LANG_ZH_HANT: return "繁體中文";
        case NW_LANG_JA: return "日本語";
        default: return "Español";
    }
}

static void set_language(enum nw_language language) {
    cached_language = language;
    cached_language_initialized = 1;
    Class defaults_class = objc_getClass("NSUserDefaults");
    id defaults = ((id (*)(id, SEL))objc_msgSend)(
        defaults_class, sel_registerName("standardUserDefaults"));
    ((void (*)(id, SEL, id, id))objc_msgSend)(
        defaults, sel_registerName("setObject:forKey:"),
        string_from_utf8(language_code(language)),
        string_from_utf8("NukeWirelessLanguage"));
    ((BOOL (*)(id, SEL))objc_msgSend)(
        defaults, sel_registerName("synchronize"));
}

static const char *tr7(const char *es, const char *en, const char *fr,
                       const char *de, const char *zh_hans,
                       const char *zh_hant, const char *ja) {
    switch (current_language()) {
        case NW_LANG_EN: return en;
        case NW_LANG_FR: return fr;
        case NW_LANG_DE: return de;
        case NW_LANG_ZH_HANS: return zh_hans;
        case NW_LANG_ZH_HANT: return zh_hant;
        case NW_LANG_JA: return ja;
        default: return es;
    }
}

static id localized_surface_title(id title) {
    const char *value = utf8(title);
    if (!value) return title;
    if (equals(value, "Wi-Fi") || equals(value, "WiFi") ||
        equals(value, "Wifi"))
        return string_from_utf8("Wi-Fi");
    if (equals(value, "Settings") || equals(value, "Ajustes") ||
        equals(value, "Hotspot") || equals(value, "Punto de acceso") ||
        equals(value, "Réglages") || equals(value, "Point d’accès") ||
        equals(value, "Einstellungen") || equals(value, "热点") ||
        equals(value, "熱點") || equals(value, "ホットスポット"))
        return string_from_utf8(tr7(
            "Punto de acceso", "Hotspot", "Point d’accès", "Hotspot",
            "热点", "熱點", "ホットスポット"));
    if (equals(value, "Info") || equals(value, "Infos") ||
        equals(value, "Information") || equals(value, "信息") ||
        equals(value, "資訊") || equals(value, "情報"))
        return string_from_utf8(tr7(
            "Info", "Info", "Infos", "Info", "信息", "資訊", "情報"));
    return title;
}

static id localized_device_action_text(id text) {
    const char *value = utf8(text);
    if (!value) return text;
    if (equals(value, "Block device") || equals(value, "Block Device"))
        return string_from_utf8(tr7(
            "Bloquear equipo", "Block device", "Bloquer l’appareil",
            "Gerät blockieren", "阻止设备", "封鎖裝置", "端末をブロック"));
    if (equals(value, "Unblock device") || equals(value, "Unblock Device"))
        return string_from_utf8(tr7(
            "Desbloquear equipo", "Unblock device", "Débloquer l’appareil",
            "Gerät entsperren", "解除阻止", "解除封鎖", "端末のブロックを解除"));
    if (equals(value, "Rename device") || equals(value, "Rename Device"))
        return string_from_utf8(tr7(
            "Renombrar equipo", "Rename device", "Renommer l’appareil",
            "Gerät umbenennen", "重命名设备", "重新命名裝置", "端末名を変更"));
    if (equals(value, "Clear nickname") || equals(value, "Clear Nickname"))
        return string_from_utf8(tr7(
            "Quitar nombre", "Clear nickname", "Supprimer le surnom",
            "Spitznamen löschen", "清除昵称", "清除暱稱", "ニックネームを削除"));
    if (equals(value, "Dismiss"))
        return string_from_utf8(tr7(
            "Cerrar", "Dismiss", "Fermer", "Schließen",
            "关闭", "關閉", "閉じる"));
    return text;
}

static void patched_tab_item_set_title(id self, SEL cmd, id title) {
    original_tab_item_set_title(self, cmd, localized_surface_title(title));
}

static void patched_navigation_item_set_title(id self, SEL cmd, id title) {
    original_navigation_item_set_title(
        self, cmd, localized_surface_title(title));
}

static void patched_controller_set_title(id self, SEL cmd, id title) {
    original_controller_set_title(self, cmd, localized_surface_title(title));
}

static id stored_name_for_mac(const char *group, const char *mac) {
    if (!mac || !mac[0]) return 0;
    Class defaults_class = objc_getClass("NSUserDefaults");
    id defaults = ((id (*)(id, SEL))objc_msgSend)(defaults_class,
        sel_registerName("standardUserDefaults"));
    id aliases = ((id (*)(id, SEL, id))objc_msgSend)(defaults,
        sel_registerName("dictionaryForKey:"),
        string_from_utf8(group));
    if (!aliases && equals(group, "NukeWirelessDeviceAliases"))
        aliases = ((id (*)(id, SEL, id))objc_msgSend)(defaults,
            sel_registerName("dictionaryForKey:"), string_from_utf8("HarpyRHDeviceAliases"));
    if (!aliases && equals(group, "NukeWirelessResolvedNames"))
        aliases = ((id (*)(id, SEL, id))objc_msgSend)(defaults,
            sel_registerName("dictionaryForKey:"), string_from_utf8("HarpyRHResolvedNames"));
    return aliases ? ((id (*)(id, SEL, id))objc_msgSend)(aliases,
        sel_registerName("objectForKey:"), string_from_utf8(mac)) : 0;
}

static void save_name_for_mac(const char *group, const char *mac, id alias) {
    if (!mac || !mac[0]) return;
    Class defaults_class = objc_getClass("NSUserDefaults");
    id defaults = ((id (*)(id, SEL))objc_msgSend)(defaults_class,
        sel_registerName("standardUserDefaults"));
    id existing = ((id (*)(id, SEL, id))objc_msgSend)(defaults,
        sel_registerName("dictionaryForKey:"),
        string_from_utf8(group));
    Class mutable_class = objc_getClass("NSMutableDictionary");
    id aliases = existing ? ((id (*)(id, SEL))objc_msgSend)(existing,
        sel_registerName("mutableCopy")) :
        ((id (*)(id, SEL))objc_msgSend)(mutable_class,
            sel_registerName("new"));
    if (alias && utf8(alias) && utf8(alias)[0])
        ((void (*)(id, SEL, id, id))objc_msgSend)(aliases,
            sel_registerName("setObject:forKey:"), alias,
            string_from_utf8(mac));
    else
        ((void (*)(id, SEL, id))objc_msgSend)(aliases,
            sel_registerName("removeObjectForKey:"), string_from_utf8(mac));
    ((void (*)(id, SEL, id, id))objc_msgSend)(defaults,
        sel_registerName("setObject:forKey:"), aliases,
        string_from_utf8(group));
    ((void (*)(id, SEL))objc_msgSend)(aliases, sel_registerName("release"));
}

static id alias_for_mac(const char *mac) {
    return stored_name_for_mac("NukeWirelessDeviceAliases", mac);
}

static void save_alias_for_mac(const char *mac, id alias) {
    save_name_for_mac("NukeWirelessDeviceAliases", mac, alias);
}

static int wifi_has_ipv6(void) {
    struct ifaddrs *first = 0;
    if (getifaddrs(&first) != 0) return 0;
    int found = 0;
    for (struct ifaddrs *item = first; item; item = item->ifa_next) {
        if (!item->ifa_addr || !equals(item->ifa_name, "en0") ||
            item->ifa_addr->sa_family != AF_INET6) continue;
        const struct sockaddr_in6 *address = (const struct sockaddr_in6 *)item->ifa_addr;
        const struct in6_addr *v6 = &address->sin6_addr;
        if (!IN6_IS_ADDR_LINKLOCAL(v6) && !IN6_IS_ADDR_LOOPBACK(v6) &&
            !IN6_IS_ADDR_UNSPECIFIED(v6) && !IN6_IS_ADDR_MULTICAST(v6)) {
            found = 1;
            break;
        }
    }
    freeifaddrs(first);
    return found;
}

static void debug_line(const char *label, const char *value) {
    int fd = open("/var/mobile/nukewireless_debug.log", 0x209, 0666);
    if (fd < 0) return;
    char line[512];
    int n = snprintf(line, sizeof(line), "%s: %s\n", label, value ? value : "(null)");
    if (n > 0 && n < (int)sizeof(line)) write(fd, line, (unsigned long)n);
    close(fd);
}

struct name_lookup_request { char ip[32]; char mac[32]; };

static void resolve_name_in_background(void *context) {
    struct name_lookup_request *request = context;
    Class pool_class = objc_getClass("NSAutoreleasePool");
    id pool = ((id (*)(id, SEL))objc_msgSend)(pool_class,
        sel_registerName("new"));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_len = sizeof(address);
    address.sin_family = AF_INET;
    char name[256] = {0};
    if (inet_pton(AF_INET, request->ip, &address.sin_addr) == 1 &&
        getnameinfo((struct sockaddr *)&address, sizeof(address), name,
            sizeof(name), 0, 0, NI_NAMEREQD) == 0 &&
        name[0] && !equals(name, request->ip)) {
        save_name_for_mac("NukeWirelessResolvedNames", request->mac,
            string_from_utf8(name));
        debug_line("resolved-name", name);
    }
    ((void (*)(id, SEL))objc_msgSend)(pool, sel_registerName("drain"));
    free(request);
}

static void schedule_name_lookup(const char *ip, const char *mac) {
    if (!ip || !mac || !ip[0] || !mac[0] ||
        stored_name_for_mac("NukeWirelessResolvedNames", mac)) return;
    for (int i = 0; i < name_lookup_attempt_count; ++i)
        if (equals(name_lookup_attempted[i], mac)) return;
    if (name_lookup_attempt_count >= 64) return;
    snprintf(name_lookup_attempted[name_lookup_attempt_count++], 32, "%s", mac);
    if (!name_lookup_queue)
        name_lookup_queue = dispatch_queue_create("nukewireless.name-lookup", 0);
    if (!name_lookup_queue) return;
    struct name_lookup_request *request = malloc(sizeof(*request));
    if (!request) return;
    snprintf(request->ip, sizeof(request->ip), "%s", ip);
    snprintf(request->mac, sizeof(request->mac), "%s", mac);
    dispatch_async_f(name_lookup_queue, request, resolve_name_in_background);
}

static int get_gateway_from_system_configuration(char *ip, unsigned long ip_size) {
    void *library = dlopen("/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", 1);
    if (!library) { debug_line("sc-dlopen", dlerror()); return 0; }
    id (*create)(id, id, void *, void *) = (void *)dlsym(library, "SCDynamicStoreCreate");
    id (*copy_value)(id, id) = (void *)dlsym(library, "SCDynamicStoreCopyValue");
    if (!create || !copy_value) { debug_line("sc-symbol", "missing"); return 0; }
    id store = create(0, string_from_utf8("NukeWireless"), 0, 0);
    if (!store) { debug_line("sc-store", "nil"); return 0; }
    id value = copy_value(store, string_from_utf8("State:/Network/Global/IPv4"));
    if (!value) { debug_line("sc-value", "nil"); CFRelease(store); return 0; }
    id router = ((id (*)(id, SEL, id))objc_msgSend)(value,
        sel_registerName("objectForKey:"), string_from_utf8("Router"));
    id primary = ((id (*)(id, SEL, id))objc_msgSend)(value,
        sel_registerName("objectForKey:"), string_from_utf8("PrimaryInterface"));
    const char *router_text = utf8(router);
    const char *primary_text = utf8(primary);
    debug_line("sc-router", router_text);
    debug_line("sc-interface", primary_text);
    int good = 0;
    if (equals(primary_text, "en0") && router_text) {
        unsigned long n = 0;
        while (((*router_text >= '0' && *router_text <= '9') || *router_text == '.') && n + 1 < ip_size)
            ip[n++] = *router_text++;
        ip[n] = 0;
        good = n >= 7;
    }
    CFRelease(value);
    CFRelease(store);
    return good;
}

static int get_mac_from_arp_table(const char *ip, char *mac, unsigned long mac_size) {
    struct in_addr wanted;
    if (inet_pton(AF_INET, ip, &wanted) != 1) return 0;
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
    size_t length = 0;
    if (sysctl(mib, 6, 0, &length, 0, 0) != 0 || !length) {
        debug_line("arp-sysctl", "size query failed");
        return 0;
    }
    char size_text[32];
    snprintf(size_text, sizeof(size_text), "%lu", (unsigned long)length);
    debug_line("arp-sysctl-size", size_text);
    void *buffer = malloc(length + 8192);
    if (!buffer) return 0;
    length += 8192;
    if (sysctl(mib, 6, buffer, &length, 0, 0) != 0) {
        debug_line("arp-sysctl", "table query failed");
        free(buffer);
        return 0;
    }
    char *cursor = buffer;
    char *end = cursor + length;
    int found = 0;
    while (cursor + sizeof(struct rt_msghdr) <= end) {
        struct rt_msghdr *message = (struct rt_msghdr *)cursor;
        if (message->rtm_msglen < sizeof(struct rt_msghdr) ||
            cursor + message->rtm_msglen > end) break;
        char *next = cursor + message->rtm_msglen;
        char *address_cursor = cursor + sizeof(struct rt_msghdr);
        struct sockaddr *destination = 0;
        struct sockaddr *gateway = 0;
        for (int index = 0; index < RTAX_MAX && address_cursor + 2 <= next; ++index) {
            if (!(message->rtm_addrs & (1 << index))) continue;
            struct sockaddr *address = (struct sockaddr *)address_cursor;
            unsigned long step = address->sa_len ? ((address->sa_len + 7) & ~7UL) : 8;
            if (address_cursor + step > next) break;
            if (index == RTAX_DST) destination = address;
            if (index == RTAX_GATEWAY) gateway = address;
            address_cursor += step;
        }
        if (destination && gateway && destination->sa_family == AF_INET &&
            gateway->sa_family == AF_LINK &&
            destination->sa_len >= sizeof(struct sockaddr_in) &&
            gateway->sa_len >= sizeof(struct sockaddr_dl)) {
            struct sockaddr_in *d = (struct sockaddr_in *)destination;
            struct sockaddr_dl *g = (struct sockaddr_dl *)gateway;
            if (d->sin_addr.s_addr == wanted.s_addr && g->sdl_alen == 6 &&
                mac_size >= 18 &&
                (char *)LLADDR(g) + 6 <= (char *)gateway + gateway->sa_len) {
                const unsigned char *bytes = (const unsigned char *)LLADDR(g);
                snprintf(mac, mac_size, "%02x:%02x:%02x:%02x:%02x:%02x",
                    bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
                found = 1;
                break;
            }
        }
        cursor = next;
    }
    free(buffer);
    debug_line("arp-sysctl-mac", found ? mac : "not found");
    return found;
}

static int get_interface_mac(const char *name, char *mac, unsigned long mac_size) {
    struct ifaddrs *first = 0;
    if (getifaddrs(&first) != 0) return 0;
    int found = 0;
    for (struct ifaddrs *item = first; item; item = item->ifa_next) {
        if (!item->ifa_addr || !equals(item->ifa_name, name) ||
            item->ifa_addr->sa_family != AF_LINK) continue;
        struct sockaddr_dl *link = (struct sockaddr_dl *)item->ifa_addr;
        if (link->sdl_alen != 6 || mac_size < 18) continue;
        const unsigned char *bytes = (const unsigned char *)LLADDR(link);
        snprintf(mac, mac_size, "%02x:%02x:%02x:%02x:%02x:%02x",
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
        found = 1;
        break;
    }
    freeifaddrs(first);
    return found;
}

static int get_interface_ip(const char *name, char *ip, unsigned long ip_size) {
    struct ifaddrs *first = 0;
    if (getifaddrs(&first) != 0) return 0;
    int found = 0;
    for (struct ifaddrs *item = first; item; item = item->ifa_next) {
        if (!item->ifa_addr || !equals(item->ifa_name, name) ||
            item->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *address = (struct sockaddr_in *)item->ifa_addr;
        if (inet_ntop(AF_INET, &address->sin_addr, ip, (socklen_t)ip_size))
            found = 1;
        break;
    }
    freeifaddrs(first);
    return found;
}

static int get_interface_netmask(const char *name, char *mask, unsigned long mask_size) {
    struct ifaddrs *first = 0;
    if (getifaddrs(&first) != 0) return 0;
    int found = 0;
    for (struct ifaddrs *item = first; item; item = item->ifa_next) {
        if (!item->ifa_addr || !item->ifa_netmask || !equals(item->ifa_name, name) ||
            item->ifa_addr->sa_family != AF_INET) continue;
        struct sockaddr_in *address = (struct sockaddr_in *)item->ifa_netmask;
        if (inet_ntop(AF_INET, &address->sin_addr, mask, (socklen_t)mask_size))
            found = 1;
        break;
    }
    freeifaddrs(first);
    return found;
}

static int get_interface_ipv6(const char *name, char *ip, unsigned long ip_size) {
    struct ifaddrs *first = 0;
    if (getifaddrs(&first) != 0) return 0;
    int found = 0;
    char link_local[INET6_ADDRSTRLEN] = {0};
    for (struct ifaddrs *item = first; item; item = item->ifa_next) {
        if (!item->ifa_addr || !equals(item->ifa_name, name) ||
            item->ifa_addr->sa_family != AF_INET6) continue;
        struct sockaddr_in6 *address = (struct sockaddr_in6 *)item->ifa_addr;
        const struct in6_addr *v6 = &address->sin6_addr;
        if (IN6_IS_ADDR_LOOPBACK(v6) || IN6_IS_ADDR_UNSPECIFIED(v6) ||
            IN6_IS_ADDR_MULTICAST(v6)) continue;
        char candidate[INET6_ADDRSTRLEN] = {0};
        if (!inet_ntop(AF_INET6, v6, candidate, sizeof(candidate))) continue;
        if (IN6_IS_ADDR_LINKLOCAL(v6)) {
            if (!link_local[0])
                snprintf(link_local, sizeof(link_local), "%s", candidate);
            continue;
        }
        snprintf(ip, ip_size, "%s", candidate);
        found = 1;
        break;
    }
    if (!found && link_local[0]) {
        snprintf(ip, ip_size, "%s", link_local);
        found = 1;
    }
    freeifaddrs(first);
    return found;
}

static int get_current_wifi_identity(char *ssid, unsigned long ssid_size,
                                     char *bssid, unsigned long bssid_size) {
    ssid[0] = 0;
    bssid[0] = 0;
    void *library = dlopen(
        "/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", 1);
    if (library) {
        id (*copy_interfaces)(void) =
            (void *)dlsym(library, "CNCopySupportedInterfaces");
        id (*copy_info)(id) =
            (void *)dlsym(library, "CNCopyCurrentNetworkInfo");
        if (copy_interfaces && copy_info) {
            id interfaces = copy_interfaces();
            if (interfaces) {
                NSUInteger count = ((NSUInteger (*)(id, SEL))objc_msgSend)(
                    interfaces, sel_registerName("count"));
                for (NSUInteger i = 0; i < count; ++i) {
                    id interface = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
                        interfaces, sel_registerName("objectAtIndex:"), i);
                    id info = copy_info(interface);
                    if (!info) continue;
                    id ssid_value = ((id (*)(id, SEL, id))objc_msgSend)(
                        info, sel_registerName("objectForKey:"),
                        string_from_utf8("SSID"));
                    id bssid_value = ((id (*)(id, SEL, id))objc_msgSend)(
                        info, sel_registerName("objectForKey:"),
                        string_from_utf8("BSSID"));
                    const char *ssid_text = utf8(ssid_value);
                    const char *bssid_text = utf8(bssid_value);
                    if (ssid_text && ssid_text[0])
                        snprintf(ssid, ssid_size, "%s", ssid_text);
                    if (bssid_text && bssid_text[0])
                        snprintf(bssid, bssid_size, "%s", bssid_text);
                    CFRelease(info);
                    if (ssid[0] || bssid[0]) break;
                }
                CFRelease(interfaces);
            }
        }
        if (ssid[0] || bssid[0]) {
            debug_line("wifi-identity", "CaptiveNetwork");
            return 1;
        }

        id (*store_create)(id, id, void *, void *) =
            (void *)dlsym(library, "SCDynamicStoreCreate");
        id (*store_copy_value)(id, id) =
            (void *)dlsym(library, "SCDynamicStoreCopyValue");
        if (store_create && store_copy_value) {
            id store = store_create(
                0, string_from_utf8("NukeWirelessWiFiIdentity"), 0, 0);
            if (store) {
                id airport = store_copy_value(
                    store,
                    string_from_utf8("State:/Network/Interface/en0/AirPort"));
                if (airport) {
                    id ssid_value = ((id (*)(id, SEL, id))objc_msgSend)(
                        airport, sel_registerName("objectForKey:"),
                        string_from_utf8("SSID_STR"));
                    id bssid_value = ((id (*)(id, SEL, id))objc_msgSend)(
                        airport, sel_registerName("objectForKey:"),
                        string_from_utf8("BSSID"));
                    if (ssid_value &&
                        ((BOOL (*)(id, SEL, id))objc_msgSend)(
                            ssid_value, sel_registerName("isKindOfClass:"),
                            objc_getClass("NSString"))) {
                        const char *text = utf8(ssid_value);
                        if (text && text[0])
                            snprintf(ssid, ssid_size, "%s", text);
                    }
                    if (bssid_value &&
                        ((BOOL (*)(id, SEL, id))objc_msgSend)(
                            bssid_value, sel_registerName("isKindOfClass:"),
                            objc_getClass("NSString"))) {
                        const char *text = utf8(bssid_value);
                        if (text && text[0])
                            snprintf(bssid, bssid_size, "%s", text);
                    }
                    CFRelease(airport);
                }
                CFRelease(store);
            }
        }
    }
    if (ssid[0] || bssid[0]) return 1;

    void *wifi = dlopen(
        "/System/Library/PrivateFrameworks/MobileWiFi.framework/MobileWiFi", 1);
    if (!wifi) return 0;
    id (*manager_create)(id, int) =
        (void *)dlsym(wifi, "WiFiManagerClientCreate");
    id (*copy_devices)(id) =
        (void *)dlsym(wifi, "WiFiManagerClientCopyDevices");
    id (*copy_current_network)(id) =
        (void *)dlsym(wifi, "WiFiDeviceClientCopyCurrentNetwork");
    id (*network_get_ssid)(id) =
        (void *)dlsym(wifi, "WiFiNetworkGetSSID");
    id (*network_get_property)(id, id) =
        (void *)dlsym(wifi, "WiFiNetworkGetProperty");
    if (!manager_create || !copy_devices || !copy_current_network ||
        !network_get_ssid || !network_get_property)
        return 0;

    id manager = manager_create(0, 0);
    if (!manager) return 0;
    id devices = copy_devices(manager);
    if (!devices) {
        CFRelease(manager);
        return 0;
    }
    NSUInteger device_count = ((NSUInteger (*)(id, SEL))objc_msgSend)(
        devices, sel_registerName("count"));
    for (NSUInteger i = 0; i < device_count; ++i) {
        id device = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            devices, sel_registerName("objectAtIndex:"), i);
        id network = copy_current_network(device);
        if (!network) continue;
        id ssid_value = network_get_ssid(network);
        id bssid_value = network_get_property(
            network, string_from_utf8("BSSID"));
        const char *ssid_text = utf8(ssid_value);
        const char *bssid_text = utf8(bssid_value);
        if (ssid_text && ssid_text[0])
            snprintf(ssid, ssid_size, "%s", ssid_text);
        if (bssid_text && bssid_text[0])
            snprintf(bssid, bssid_size, "%s", bssid_text);
        CFRelease(network);
        if (ssid[0] || bssid[0]) break;
    }
    CFRelease(devices);
    CFRelease(manager);
    if (ssid[0] || bssid[0])
        debug_line("wifi-identity", "MobileWiFi");
    return ssid[0] || bssid[0];
}

static int get_dns_servers(char *dns, unsigned long dns_size) {
    dns[0] = 0;
    void *library = dlopen(
        "/System/Library/Frameworks/SystemConfiguration.framework/SystemConfiguration", 1);
    if (!library) return 0;
    id (*create)(id, id, void *, void *) = (void *)dlsym(library, "SCDynamicStoreCreate");
    id (*copy_value)(id, id) = (void *)dlsym(library, "SCDynamicStoreCopyValue");
    if (!create || !copy_value) return 0;
    id store = create(0, string_from_utf8("NukeWirelessInfo"), 0, 0);
    if (!store) return 0;
    id value = copy_value(store, string_from_utf8("State:/Network/Global/DNS"));
    if (!value) {
        CFRelease(store);
        return 0;
    }
    id servers = ((id (*)(id, SEL, id))objc_msgSend)(
        value, sel_registerName("objectForKey:"), string_from_utf8("ServerAddresses"));
    NSUInteger count = servers ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        servers, sel_registerName("count")) : 0;
    unsigned long used = 0;
    for (NSUInteger i = 0; i < count && i < 3; ++i) {
        id server = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            servers, sel_registerName("objectAtIndex:"), i);
        const char *text = utf8(server);
        if (!text || !*text) continue;
        int written = snprintf(dns + used, dns_size > used ? dns_size - used : 0,
            "%s%s", used ? ", " : "", text);
        if (written <= 0 || used + (unsigned long)written >= dns_size) break;
        used += (unsigned long)written;
    }
    CFRelease(value);
    CFRelease(store);
    return dns[0] != 0;
}

static id brand_for_mac(const char *mac) {
    if (!mac) return 0;
    char prefix[7] = {0};
    int digits = 0;
    for (const char *p = mac; *p && digits < 6; ++p) {
        char c = *p;
        if (c >= 'a' && c <= 'f') c -= 32;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
            prefix[digits++] = c;
    }
    if (digits != 6) return 0;
    int first = (prefix[0] <= '9' ? prefix[0] - '0' : prefix[0] - 'A' + 10) * 16 +
                (prefix[1] <= '9' ? prefix[1] - '0' : prefix[1] - 'A' + 10);
    if (first & 2) return string_from_utf8("Private MAC");
    if (!oui_brands) {
        const char *root = utf8(jailbreak_prefix());
        if (!root) return 0;
        char path[512];
        if (snprintf(path, sizeof(path),
                "%s/usr/share/nukewireless-roothide/oui_vendors.plist", root)
            >= (int)sizeof(path)) return 0;
        Class dictionary = objc_getClass("NSDictionary");
        id loaded = ((id (*)(id, SEL, id))objc_msgSend)(dictionary,
            sel_registerName("dictionaryWithContentsOfFile:"),
            string_from_utf8(path));
        if (loaded) oui_brands = ((id (*)(id, SEL))objc_msgSend)(loaded,
            sel_registerName("retain"));
        debug_line("oui-loaded", loaded ? "yes" : "no");
    }
    return oui_brands ? ((id (*)(id, SEL, id))objc_msgSend)(oui_brands,
        sel_registerName("objectForKey:"), string_from_utf8(prefix)) : 0;
}

static int get_gateway(char *ip, unsigned long ip_size, char *mac, unsigned long mac_size) {
    char line[256];
    char command[256];
    char interface[32] = {0};
    ip[0] = 0;
    mac[0] = 0;
    int sc_good = get_gateway_from_system_configuration(ip, ip_size);
    debug_line("sc-good", sc_good ? "yes" : "no");
    const char *root = utf8(jailbreak_prefix());
    debug_line("root", root);
    if (!root) return 0;
    if (!sc_good) {
        if (snprintf(command, sizeof(command), "%s/usr/sbin/route -n get default 2>&1", root) >= (int)sizeof(command)) return 0;
        debug_line("route-command", command);
        void *route = popen(command, "r");
        if (!route) { debug_line("route", "popen failed"); return 0; }
        while (fgets(line, sizeof(line), route)) {
            debug_line("route-output", line);
            const char *value = 0;
            if ((value = "gateway:"), contains(line, value)) {
                const char *p = line;
                while (*p && !starts_with(p, value)) ++p;
                p += 8;
                while (*p == ' ' || *p == '\t') ++p;
                unsigned long n = 0;
                while (((*p >= '0' && *p <= '9') || *p == '.') && n + 1 < ip_size)
                    ip[n++] = *p++;
                ip[n] = 0;
            } else if ((value = "interface:"), contains(line, value)) {
                const char *p = line;
                while (*p && !starts_with(p, value)) ++p;
                p += 10;
                while (*p == ' ' || *p == '\t') ++p;
                unsigned long n = 0;
                while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')) {
                    if (n + 1 < sizeof(interface)) interface[n++] = *p;
                    ++p;
                }
                interface[n] = 0;
            }
        }
        pclose(route);
        debug_line("route-ip", ip);
        debug_line("route-interface", interface);
        if (!equals(interface, "en0") || !ip[0]) return 0;
    }
    if (get_mac_from_arp_table(ip, mac, mac_size)) return 1;
    if (snprintf(command, sizeof(command), "%s/usr/sbin/arp -n %s 2>&1", root, ip) >= (int)sizeof(command)) return 0;
    debug_line("arp-command", command);
    void *arp = popen(command, "r");
    if (!arp) { debug_line("arp", "popen failed"); return 0; }
    while (fgets(line, sizeof(line), arp)) {
        debug_line("arp-output", line);
        const char *p = line;
        while (*p && !starts_with(p, " at ")) ++p;
        if (!*p) continue;
        p += 4;
        unsigned long n = 0;
        while (((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                (*p >= 'A' && *p <= 'F') || *p == ':') && n + 1 < mac_size)
            mac[n++] = *p++;
        mac[n] = 0;
        break;
    }
    pclose(arp);
    debug_line("arp-mac", mac);
    return mac[0] != 0;
}

static id string_from_utf8(const char *value) {
    Class string = objc_getClass("NSString");
    return ((id (*)(id, SEL, const char *))objc_msgSend)(
        string, sel_registerName("stringWithUTF8String:"), value);
}

static id jailbreak_prefix(void) {
    Class bundle_class = objc_getClass("NSBundle");
    id bundle = ((id (*)(id, SEL))objc_msgSend)(bundle_class, sel_registerName("mainBundle"));
    id bundle_path = ((id (*)(id, SEL))objc_msgSend)(bundle, sel_registerName("bundlePath"));
    id applications = ((id (*)(id, SEL))objc_msgSend)(
        bundle_path, sel_registerName("stringByDeletingLastPathComponent"));
    return ((id (*)(id, SEL))objc_msgSend)(
        applications, sel_registerName("stringByDeletingLastPathComponent"));
}

static int is_jailbreak_file(const char *path) {
    return starts_with(path, "/usr/libexec/harpy-reloaded/") ||
           starts_with(path, "/usr/bin/arpoison") ||
           starts_with(path, "/sbin/pfctl");
}

static id rewrite_path(id path) {
    if (!path) return path;
    const char *utf8 = ((const char *(*)(id, SEL))objc_msgSend)(
        path, sel_registerName("UTF8String"));
    if (!is_jailbreak_file(utf8)) return path;
    return ((id (*)(id, SEL, id))objc_msgSend)(
        jailbreak_prefix(), sel_registerName("stringByAppendingString:"), path);
}

static id replace_in_argument(id argument, const char *old_path) {
    id old_string = string_from_utf8(old_path);
    id new_string = rewrite_path(old_string);
    return ((id (*)(id, SEL, id, id))objc_msgSend)(
        argument, sel_registerName("stringByReplacingOccurrencesOfString:withString:"),
        old_string, new_string);
}

static id rewrite_argument(id argument) {
    if (!argument) return argument;
    const char *value = utf8(argument);
    if (starts_with(value, "/private/var/containers/Bundle/Application/.jbroot-") ||
        starts_with(value, "/var/containers/Bundle/Application/.jbroot-"))
        return argument;
    id result = replace_in_argument(argument, "/usr/libexec/harpy-reloaded/");
    result = replace_in_argument(result, "/usr/bin/arpoison");
    result = replace_in_argument(result, "/sbin/pfctl");
    return result;
}

static BOOL patched_exists(id self, SEL cmd, id path) {
    return original_exists(self, cmd, rewrite_path(path));
}

static void patched_launch_path(id self, SEL cmd, id path) {
    debug_line("task-launch-path", utf8(path));
    original_launch_path(self, cmd, rewrite_path(path));
}

static void patched_terminate(id self, SEL cmd) {
    id path = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("launchPath"));
    debug_line("task-terminate", utf8(path));
    original_terminate(self, cmd);
}

static void patched_interrupt(id self, SEL cmd) {
    id path = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("launchPath"));
    debug_line("task-interrupt", utf8(path));
    original_interrupt(self, cmd);
}

static void patched_launch(id self, SEL cmd) {
    original_launch(self, cmd);
    id launch_path = ((id (*)(id, SEL))objc_msgSend)(self,
        sel_registerName("launchPath"));
    debug_line("task-launched-path", utf8(launch_path));
    char launch_pid[32];
    snprintf(launch_pid, sizeof(launch_pid), "%d",
        ((int (*)(id, SEL))objc_msgSend)(self,
            sel_registerName("processIdentifier")));
    debug_line("task-launched-pid", launch_pid);
    id arguments = ((id (*)(id, SEL))objc_msgSend)(self, sel_registerName("arguments"));
    NSUInteger count = arguments ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        arguments, sel_registerName("count")) : 0;
    int arpoison = 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id item = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            arguments, sel_registerName("objectAtIndex:"), i);
        if (contains(utf8(item), "arpoison")) { arpoison = 1; break; }
    }
    if (arpoison && !repair_in_progress && pending_block.ip[0]) {
        int pid = ((int (*)(id, SEL))objc_msgSend)(
            self, sel_registerName("processIdentifier"));
        char line[96];
        snprintf(line, sizeof(line), "%s pid=%d", pending_block.ip, pid);
        debug_line("block-launched", line);
        if (pid > 0) {
            int slot = -1;
            for (int i = 0; i < 64; ++i)
                if (equals(blocks[i].ip, pending_block.ip)) { slot = i; break; }
            if (slot < 0) for (int i = 0; i < 64; ++i)
                if (!blocks[i].ip[0]) { slot = i; break; }
            if (slot >= 0) {
                blocks[slot] = pending_block;
                blocks[slot].pid = pid;
            }
        }
        memset(&pending_block, 0, sizeof(pending_block));
    }
}

static void run_as_root(const char *path, const char **args, int count) {
    Class array_class = objc_getClass("NSMutableArray");
    id array = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
        array_class, sel_registerName("arrayWithCapacity:"), (NSUInteger)count);
    for (int i = 0; i < count; ++i)
        ((void (*)(id, SEL, id))objc_msgSend)(array,
            sel_registerName("addObject:"), string_from_utf8(args[i]));
    Class task_class = objc_getClass("NSTask");
    id task = ((id (*)(id, SEL))objc_msgSend)(task_class,
        sel_registerName("alloc"));
    task = ((id (*)(id, SEL))objc_msgSend)(task,
        sel_registerName("init"));
    ((void (*)(id, SEL, id))objc_msgSend)(task,
        sel_registerName("setLaunchPath:"), string_from_utf8(path));
    ((void (*)(id, SEL, id))objc_msgSend)(task,
        sel_registerName("setArguments:"), array);
    Class commands = objc_getClass("_TtC13HarpyReloaded10MCCommands");
    captured_root_task = 0;
    capture_root_task = 1;
    if (commands)
        ((void (*)(id, SEL, id, id))objc_msgSend)(commands,
            sel_registerName("asRootWithTask:args:"), task, array);
    capture_root_task = 0;
    if (captured_root_task) {
        ((void (*)(id, SEL))objc_msgSend)(captured_root_task,
            sel_registerName("launch"));
        if (contains(path, "/kill")) {
            ((void (*)(id, SEL))objc_msgSend)(captured_root_task,
                sel_registerName("waitUntilExit"));
            char result[32];
            snprintf(result, sizeof(result), "%d",
                ((int (*)(id, SEL))objc_msgSend)(captured_root_task,
                    sel_registerName("terminationStatus")));
            debug_line("root-kill-status", result);
        }
    } else {
        debug_line("root-task", "not captured");
    }
    captured_root_task = 0;
}

static void stop_block_for_ip(const char *ip_text) {
    debug_line("unblock-ip", ip_text);
    int slot = -1;
    for (int i = 0; i < 64; ++i)
        if (equals(blocks[i].ip, ip_text)) { slot = i; break; }
    if (slot < 0 || blocks[slot].pid <= 0) {
        debug_line("unblock-pid", "missing");
        return;
    }
    char pid_text[32];
    snprintf(pid_text, sizeof(pid_text), "%d", blocks[slot].pid);
    const char *root = utf8(jailbreak_prefix());
    if (!root) return;
    char kill_path[256], poison_path[256];
    snprintf(kill_path, sizeof(kill_path), "%s/usr/bin/kill", root);
    const char *kill_args[] = {"-TERM", pid_text};
    run_as_root(kill_path, kill_args, 2);
    debug_line("unblock-kill", pid_text);
    char gateway_ip[32], gateway_mac[32];
    if (blocks[slot].real_mac[0] &&
        get_gateway(gateway_ip, sizeof(gateway_ip), gateway_mac, sizeof(gateway_mac))) {
        snprintf(poison_path, sizeof(poison_path), "%s/usr/bin/arpoison", root);
        const char *repair_args[] = {"-i", "en0", "-d", gateway_ip,
            "-s", blocks[slot].ip, "-t", gateway_mac, "-r", blocks[slot].real_mac,
            "-a", "-w", "0.2", "-n", "5"};
        repair_in_progress = 1;
        run_as_root(poison_path, repair_args, 15);
        repair_in_progress = 0;
        debug_line("unblock-repair", blocks[slot].real_mac);
    }
    memset(&blocks[slot], 0, sizeof(blocks[slot]));
}

static void patched_swift_unblock(uint64_t first, uint64_t second) {
    char raw[80];
    snprintf(raw, sizeof(raw), "%016llx %016llx",
        (unsigned long long)first, (unsigned long long)second);
    debug_line("swift-unblock-raw", raw);
    char ip[32];
    int n = 0;
    if (swift_string_to_nsstring) {
        id text = swift_string_to_nsstring(first, second);
        const char *value = utf8(text);
        while (value && n < 15 &&
            ((value[n] >= '0' && value[n] <= '9') || value[n] == '.')) {
            ip[n] = value[n];
            ++n;
        }
    }
    ip[n] = 0;
    debug_line("swift-unblock", ip);
    original_swift_unblock(first, second);
    if (n >= 7) stop_block_for_ip(ip);
}

static id block_processes_for_ip(const char *ip) {
    Class array_class = objc_getClass("NSMutableArray");
    id array = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
        array_class, sel_registerName("arrayWithCapacity:"), 2);
    for (int i = 0; i < 64; ++i) {
        if (blocks[i].pid <= 0 || (ip && !equals(blocks[i].ip, ip))) continue;
        char pid_text[32];
        snprintf(pid_text, sizeof(pid_text), "%d", blocks[i].pid);
        ((void (*)(id, SEL, id))objc_msgSend)(array,
            sel_registerName("addObject:"), string_from_utf8(pid_text));
    }
    return array;
}

static id patched_running_ip(id self, SEL cmd, id ip) {
    debug_line("running-ip", utf8(ip));
    return block_processes_for_ip(utf8(ip));
}

static id patched_running_arp(id self, SEL cmd) {
    debug_line("running-arp", "called");
    return block_processes_for_ip(0);
}

static void patched_found_device(id self, SEL cmd, id device) {
    const char *ip = utf8(((id (*)(id, SEL))objc_msgSend)(
        device, sel_registerName("ipAddress")));
    const char *name = utf8(((id (*)(id, SEL))objc_msgSend)(
        device, sel_registerName("hostname")));
    const char *mac = utf8(((id (*)(id, SEL))objc_msgSend)(
        device, sel_registerName("macAddress")));
    const char *brand = utf8(((id (*)(id, SEL))objc_msgSend)(
        device, sel_registerName("brand")));
    debug_line("scan-device-ip", ip);
    debug_line("scan-device-name", name);
    debug_line("scan-device-mac", mac);
    debug_line("scan-device-brand", brand);
    char local_ip[32] = {0};
    if (ip && get_interface_ip("en0", local_ip, sizeof(local_ip)) &&
        equals(ip, local_ip) && name && name[0] && !equals(name, "Unknown Host"))
        scanned_count = 0; /* The local entry starts a fresh Wi-Fi scan. */
    if (ip && get_interface_ip("en0", local_ip, sizeof(local_ip)) &&
        equals(ip, local_ip) && (!name || !name[0] ||
            equals(name, "Unknown Host"))) {
        debug_line("scan-device", "skipped own Wi-Fi address");
        return;
    }
    id alias = alias_for_mac(mac);
    id resolved = stored_name_for_mac("NukeWirelessResolvedNames", mac);
    if (alias && utf8(alias) && utf8(alias)[0]) {
        ((void (*)(id, SEL, id))objc_msgSend)(device,
            sel_registerName("setHostname:"), alias);
        debug_line("scan-device-alias", utf8(alias));
    } else if (ip && (!name || !name[0] || equals(name, "Unknown Host")) &&
               resolved && utf8(resolved) && utf8(resolved)[0]) {
        ((void (*)(id, SEL, id))objc_msgSend)(device,
            sel_registerName("setHostname:"), resolved);
        debug_line("scan-device-resolved", utf8(resolved));
    } else if (ip && (!name || !name[0] || equals(name, "Unknown Host"))) {
        const char *last = ip;
        for (const char *p = ip; *p; ++p) if (*p == '.') last = p + 1;
        char label[64];
        snprintf(label, sizeof(label), "Equipo .%s", last);
        ((void (*)(id, SEL, id))objc_msgSend)(device,
            sel_registerName("setHostname:"), string_from_utf8(label));
        debug_line("scan-device-label", label);
        schedule_name_lookup(ip, mac);
    }
    if (!brand || !brand[0] || equals(brand, "Unknown Brand")) {
        id vendor = brand_for_mac(mac);
        if (vendor) {
            ((void (*)(id, SEL, id))objc_msgSend)(device,
                sel_registerName("setBrand:"), vendor);
            debug_line("scan-device-vendor", utf8(vendor));
        }
    }
    if (ip && mac) {
        int known = -1;
        for (int i = 0; i < scanned_count; ++i)
            if (equals(scanned_devices[i].ip, ip)) { known = i; break; }
        if (known < 0 && scanned_count < 64) known = scanned_count++;
        if (known >= 0) {
            struct scanned_device *item = &scanned_devices[known];
            snprintf(item->ip, sizeof(item->ip), "%s", ip);
            snprintf(item->mac, sizeof(item->mac), "%s", mac);
            const char *final_name = utf8(((id (*)(id, SEL))objc_msgSend)(
                device, sel_registerName("hostname")));
            snprintf(item->name, sizeof(item->name), "%s",
                final_name ? final_name : "");
        }
    }
    original_found_device(self, cmd, device);
}

static id patched_scanner_init(id self, SEL cmd, id delegate, BOOL hotspot) {
    id result = original_scanner_init(self, cmd, delegate, hotspot);
    char line[160];
    snprintf(line, sizeof(line), "self=%p result=%p delegate=%p hotspot=%d",
        self, result, delegate, hotspot ? 1 : 0);
    debug_line("scanner-init", line);
    if (result && !hotspot) {
        if (wifi_scanner && wifi_scanner != result)
            ((void (*)(id, SEL))objc_msgSend)(wifi_scanner,
                sel_registerName("release"));
        wifi_scanner = ((id (*)(id, SEL))objc_msgSend)(result,
            sel_registerName("retain"));
        snprintf(line, sizeof(line), "captured=%p", wifi_scanner);
        debug_line("wifi-scanner", line);
    }
    return result;
}

static void patched_scanner_start(id self, SEL cmd) {
    char line[128];
    BOOL before = ((BOOL (*)(id, SEL))objc_msgSend)(self,
        sel_registerName("isScanning"));
    snprintf(line, sizeof(line), "self=%p wifi=%p before=%d",
        self, wifi_scanner, before ? 1 : 0);
    debug_line("scanner-start-enter", line);
    if (self == wifi_scanner) {
        scanned_count = 0;
        scan_in_progress = 1;
        scan_timed_out = 0;
        scan_finished_at = 0;
        debug_line("wifi-scan", "started");
    }
    original_scanner_start(self, cmd);
    BOOL after = ((BOOL (*)(id, SEL))objc_msgSend)(self,
        sel_registerName("isScanning"));
    snprintf(line, sizeof(line), "self=%p wifi=%p after=%d",
        self, wifi_scanner, after ? 1 : 0);
    debug_line("scanner-start-exit", line);
}

static void patched_scanner_finished(id self, SEL cmd, NSUInteger status) {
    char line[128];
    snprintf(line, sizeof(line), "self=%p wifi=%p status=%lu",
        self, wifi_scanner, status);
    debug_line("scanner-finished-enter", line);
    original_scanner_finished(self, cmd, status);
    if (self == wifi_scanner) {
        scan_in_progress = 0;
        scan_timed_out = 0;
        scan_finished_at = time(0);
        dispatch_async_f(dispatch_get_main_queue(), 0, scan_finished_on_main);
        debug_line("wifi-scan", "finished");
    }
}

static void patched_scanner_failed(id self, SEL cmd) {
    char line[128];
    snprintf(line, sizeof(line), "self=%p wifi=%p", self, wifi_scanner);
    debug_line("scanner-failed-enter", line);
    original_scanner_failed(self, cmd);
    if (self == wifi_scanner) {
        scan_in_progress = 0;
        scan_timed_out = 1;
        bulk_confirm_pending = 0;
        dispatch_async_f(dispatch_get_main_queue(), 0, scan_finished_on_main);
        debug_line("wifi-scan", "failed");
    }
}

static void set_bulk_title(const char *title) {
    if (bulk_button)
        ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(bulk_button,
            sel_registerName("setTitle:forState:"), string_from_utf8(title), 0);
}

static int pid_is_alive(int pid) {
    return pid > 0 && (kill(pid, 0) == 0 || errno == EPERM);
}

static int valid_mac(const char *mac) {
    if (!mac || strlen(mac) != 17) return 0;
    for (int i = 0; i < 17; ++i) {
        char c = mac[i];
        if (i % 3 == 2) { if (c != ':') return 0; }
        else if (!((c >= '0' && c <= '9') ||
                   (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F'))) return 0;
    }
    return 1;
}

static int running_block_count(void) {
    int count = 0;
    for (int i = 0; i < 64; ++i)
        if (pid_is_alive(blocks[i].pid)) ++count;
    return count;
}

static void update_bulk_button_title(void) {
    if (bulk_count) {
        set_bulk_title(tr7("Desbloquear todos", "Unblock all", "Tout débloquer",
            "Alle entsperren", "全部解除阻止", "全部解除封鎖", "すべて解除"));
        return;
    }
    if (scan_in_progress && bulk_confirm_pending) {
        set_bulk_title(tr7("Actualizando lista...", "Refreshing list...",
            "Actualisation...", "Liste wird aktualisiert...",
            "正在刷新列表...", "正在重新整理列表...", "一覧を更新中..."));
        return;
    }
    if (bulk_confirm_count && time(0) <= bulk_confirm_until) {
        char title[80];
        snprintf(title, sizeof(title), "%s (%d)",
            tr7("Confirmar bloqueo", "Confirm block", "Confirmer le blocage",
                "Blockierung bestätigen", "确认阻止", "確認封鎖", "ブロックを確認"),
            bulk_confirm_count);
        set_bulk_title(title);
        return;
    }
    set_bulk_title(tr7("Bloquear todos", "Block all", "Tout bloquer",
        "Alle blockieren", "全部阻止", "全部封鎖", "すべてブロック"));
}

static const char *dashboard_text_for_language(
    enum nw_language language,
    const char *es, const char *en, const char *fr, const char *de,
    const char *zh_hans, const char *zh_hant, const char *ja) {
    switch (language) {
        case NW_LANG_EN: return en;
        case NW_LANG_FR: return fr;
        case NW_LANG_DE: return de;
        case NW_LANG_ZH_HANS: return zh_hans;
        case NW_LANG_ZH_HANT: return zh_hant;
        case NW_LANG_JA: return ja;
        default: return es;
    }
}

static void localize_dashboard_buttons(enum nw_language language) {
    if (bulk_count) {
        set_bulk_title(dashboard_text_for_language(language,
            "Desbloquear todos", "Unblock all", "Tout débloquer",
            "Alle entsperren", "全部解除阻止", "全部解除封鎖", "すべて解除"));
    } else if (scan_in_progress && bulk_confirm_pending) {
        set_bulk_title(dashboard_text_for_language(language,
            "Actualizando lista...", "Refreshing list...", "Actualisation...",
            "Liste wird aktualisiert...", "正在刷新列表...",
            "正在重新整理列表...", "一覧を更新中..."));
    } else if (bulk_confirm_count && time(0) <= bulk_confirm_until) {
        char title[80];
        snprintf(title, sizeof(title), "%s (%d)",
            dashboard_text_for_language(language,
                "Confirmar bloqueo", "Confirm block", "Confirmer le blocage",
                "Blockierung bestätigen", "确认阻止", "確認封鎖",
                "ブロックを確認"),
            bulk_confirm_count);
        set_bulk_title(title);
    } else {
        set_bulk_title(dashboard_text_for_language(language,
            "Bloquear todos", "Block all", "Tout bloquer",
            "Alle blockieren", "全部阻止", "全部封鎖", "すべてブロック"));
    }
    if (alias_button)
        ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
            alias_button, sel_registerName("setTitle:forState:"),
            string_from_utf8(dashboard_text_for_language(language,
                "Nombres", "Names", "Noms", "Namen",
                "名称", "名稱", "名前")), 0);
}

static int bulk_eligible(struct scanned_device *out, int capacity) {
    char local_ip[32] = {0}, router_ip[32] = {0}, router_mac[32] = {0};
    if (!get_interface_ip("en0", local_ip, sizeof(local_ip)) ||
        !get_gateway(router_ip, sizeof(router_ip), router_mac,
            sizeof(router_mac))) return 0;
    int count = 0;
    for (int i = 0; i < scanned_count && count < capacity; ++i) {
        struct scanned_device *d = &scanned_devices[i];
        struct in_addr parsed;
        if (inet_pton(AF_INET, d->ip, &parsed) != 1 || !valid_mac(d->mac) ||
            equals(d->ip, local_ip) ||
            equals(d->ip, router_ip)) continue;
        out[count++] = *d;
    }
    return count;
}

static void render_dashboard_controls(id label, id block_button, id names_button) {
    if (!label) return;
    struct scanned_device eligible[64];
    int available = bulk_eligible(eligible, 64);
    int active = running_block_count();
    char status[256];
    const char *ipv6_note = tr7(
        "IPv6 presente · bloqueo solo IPv4",
        "IPv6 present · IPv4 blocking only",
        "IPv6 présent · blocage IPv4 uniquement",
        "IPv6 vorhanden · nur IPv4-Blockierung",
        "检测到 IPv6 · 仅阻止 IPv4",
        "偵測到 IPv6 · 僅封鎖 IPv4",
        "IPv6あり · IPv4のみブロック");
    const char *refresh_note = tr7(
        "Desliza hacia abajo para actualizar",
        "Pull down to refresh",
        "Tirez vers le bas pour actualiser",
        "Zum Aktualisieren nach unten ziehen",
        "下拉刷新",
        "下拉重新整理",
        "下に引いて更新");
    const char *retry_note = tr7(
        "Desliza hacia abajo para reintentar",
        "Pull down to try again",
        "Tirez vers le bas pour réessayer",
        "Zum erneuten Versuch nach unten ziehen",
        "下拉重试",
        "下拉重試",
        "下に引いて再試行");
    const char *failed_note = tr7(
        " · algunos fallaron",
        " · some failed",
        " · certains ont échoué",
        " · einige fehlgeschlagen",
        " · 部分失败",
        " · 部分失敗",
        " · 一部失敗");
    if (scan_in_progress)
        snprintf(status, sizeof(status), "%s\n%s",
            tr7("Actualizando equipos...", "Refreshing devices...",
                "Actualisation des appareils...", "Geräte werden aktualisiert...",
                "正在刷新设备...", "正在重新整理裝置...", "端末を更新中..."),
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    else if (scan_timed_out)
        snprintf(status, sizeof(status), "%s\n%s",
            tr7("No se pudo terminar el escaneo", "The scan could not be completed",
                "L’analyse n’a pas pu être terminée", "Der Scan konnte nicht abgeschlossen werden",
                "无法完成扫描", "無法完成掃描", "スキャンを完了できませんでした"),
            retry_note);
    else if (bulk_count)
        snprintf(status, sizeof(status), "%d %s · %d %s%s\n%s",
            available,
            tr7("equipos", "devices", "appareils", "Geräte", "台设备", "部裝置", "台"),
            active,
            tr7("procesos activos", "active processes", "processus actifs",
                "aktive Prozesse", "个活动进程", "個活動程序", "件の有効なプロセス"),
            bulk_last_failed ? failed_note : "",
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    else
        snprintf(status, sizeof(status), "%d %s · %d %s\n%s",
            available,
            tr7("equipos", "devices", "appareils", "Geräte", "台设备", "部裝置", "台"),
            active,
            tr7("procesos activos", "active processes", "processus actifs",
                "aktive Prozesse", "个活动进程", "個活動程序", "件の有効なプロセス"),
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    ((void (*)(id, SEL, id))objc_msgSend)(label,
        sel_registerName("setText:"), string_from_utf8(status));
    if (names_button)
        ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
            names_button, sel_registerName("setTitle:forState:"),
            string_from_utf8(tr7("Nombres", "Names", "Noms", "Namen",
                "名称", "名稱", "名前")), 0);
    if (block_button) {
        const char *title = 0;
        char confirm_title[80];
        if (bulk_count) {
            title = tr7("Desbloquear todos", "Unblock all", "Tout débloquer",
                "Alle entsperren", "全部解除阻止", "全部解除封鎖", "すべて解除");
        } else if (scan_in_progress && bulk_confirm_pending) {
            title = tr7("Actualizando lista...", "Refreshing list...",
                "Actualisation...", "Liste wird aktualisiert...",
                "正在刷新列表...", "正在重新整理列表...", "一覧を更新中...");
        } else if (bulk_confirm_count && time(0) <= bulk_confirm_until) {
            snprintf(confirm_title, sizeof(confirm_title), "%s (%d)",
                tr7("Confirmar bloqueo", "Confirm block", "Confirmer le blocage",
                    "Blockierung bestätigen", "确认阻止", "確認封鎖",
                    "ブロックを確認"), bulk_confirm_count);
            title = confirm_title;
        } else {
            title = tr7("Bloquear todos", "Block all", "Tout bloquer",
                "Alle blockieren", "全部阻止", "全部封鎖", "すべてブロック");
        }
        ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
            block_button, sel_registerName("setTitle:forState:"),
            string_from_utf8(title), 0);
    }
}

static void update_dashboard(void) {
    if (!status_label) return;
    struct scanned_device eligible[64];
    int available = bulk_eligible(eligible, 64);
    int active = running_block_count();
    char status[256];
    const char *ipv6_note = tr7(
        "IPv6 presente · bloqueo solo IPv4",
        "IPv6 present · IPv4 blocking only",
        "IPv6 présent · blocage IPv4 uniquement",
        "IPv6 vorhanden · nur IPv4-Blockierung",
        "检测到 IPv6 · 仅阻止 IPv4",
        "偵測到 IPv6 · 僅封鎖 IPv4",
        "IPv6あり · IPv4のみブロック");
    const char *refresh_note = tr7(
        "Desliza hacia abajo para actualizar",
        "Pull down to refresh",
        "Tirez vers le bas pour actualiser",
        "Zum Aktualisieren nach unten ziehen",
        "下拉刷新",
        "下拉重新整理",
        "下に引いて更新");
    const char *retry_note = tr7(
        "Desliza hacia abajo para reintentar",
        "Pull down to try again",
        "Tirez vers le bas pour réessayer",
        "Zum erneuten Versuch nach unten ziehen",
        "下拉重试",
        "下拉重試",
        "下に引いて再試行");
    const char *failed_note = tr7(
        " · algunos fallaron",
        " · some failed",
        " · certains ont échoué",
        " · einige fehlgeschlagen",
        " · 部分失败",
        " · 部分失敗",
        " · 一部失敗");
    if (scan_in_progress)
        snprintf(status, sizeof(status), "%s\n%s",
            tr7("Actualizando equipos...", "Refreshing devices...",
                "Actualisation des appareils...", "Geräte werden aktualisiert...",
                "正在刷新设备...", "正在重新整理裝置...", "端末を更新中..."),
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    else if (scan_timed_out)
        snprintf(status, sizeof(status), "%s\n%s",
            tr7("No se pudo terminar el escaneo", "The scan could not be completed",
                "L’analyse n’a pas pu être terminée", "Der Scan konnte nicht abgeschlossen werden",
                "无法完成扫描", "無法完成掃描", "スキャンを完了できませんでした"),
            retry_note);
    else if (bulk_count)
        snprintf(status, sizeof(status), "%d %s · %d %s%s\n%s",
            available,
            tr7("equipos", "devices", "appareils", "Geräte", "台设备", "部裝置", "台"),
            active,
            tr7("procesos activos", "active processes", "processus actifs",
                "aktive Prozesse", "个活动进程", "個活動程序", "件の有効なプロセス"),
            bulk_last_failed ? failed_note : "",
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    else
        snprintf(status, sizeof(status), "%d %s · %d %s\n%s",
            available,
            tr7("equipos", "devices", "appareils", "Geräte", "台设备", "部裝置", "台"),
            active,
            tr7("procesos activos", "active processes", "processus actifs",
                "aktive Prozesse", "个活动进程", "個活動程序", "件の有効なプロセス"),
            wifi_has_ipv6() ? ipv6_note : refresh_note);
    ((void (*)(id, SEL, id))objc_msgSend)(status_label,
        sel_registerName("setText:"), string_from_utf8(status));
    if (alias_button)
        ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
            alias_button, sel_registerName("setTitle:forState:"),
            string_from_utf8(tr7("Nombres", "Names", "Noms", "Namen",
                "名称", "名稱", "名前")), 0);
    if (refresh_control) {
        Class attr_class = objc_getClass("NSAttributedString");
        id caption = ((id (*)(id, SEL))objc_msgSend)(attr_class,
            sel_registerName("alloc"));
        caption = ((id (*)(id, SEL, id))objc_msgSend)(caption,
            sel_registerName("initWithString:"),
            string_from_utf8(tr7("Actualizando equipos...", "Refreshing devices...",
                "Actualisation des appareils...", "Geräte werden aktualisiert...",
                "正在刷新设备...", "正在重新整理裝置...", "端末を更新中...")));
        ((void (*)(id, SEL, id))objc_msgSend)(refresh_control,
            sel_registerName("setAttributedTitle:"), caption);
        ((void (*)(id, SEL))objc_msgSend)(caption, sel_registerName("release"));
    }
    update_bulk_button_title();
}

static void scan_finished_on_main(void *context) {
    (void)context;
    if (refresh_control)
        ((void (*)(id, SEL))objc_msgSend)(refresh_control,
            sel_registerName("endRefreshing"));
    if (scan_timed_out) {
        bulk_confirm_pending = 0;
        bulk_confirm_count = 0;
    } else if (bulk_confirm_pending) {
        confirmed_count = bulk_eligible(confirmed_devices, 64);
        bulk_confirm_count = confirmed_count;
        bulk_confirm_until = time(0) + 15;
        bulk_confirm_pending = 0;
    } else {
        bulk_confirm_count = 0;
    }
    update_dashboard();
}

static int request_wifi_scan(void) {
    if (!wifi_scanner || !class_getInstanceMethod(object_getClass(wifi_scanner),
        sel_registerName("start"))) {
        debug_line("wifi-scan", "scanner unavailable");
        return 0;
    }
    scan_in_progress = 1;
    scan_timed_out = 0;
    scan_requested_at = time(0);
    char line[128];
    BOOL before = ((BOOL (*)(id, SEL))objc_msgSend)(wifi_scanner,
        sel_registerName("isScanning"));
    snprintf(line, sizeof(line), "scanner=%p before=%d", wifi_scanner,
        before ? 1 : 0);
    debug_line("request-scan-before", line);
    ((void (*)(id, SEL))objc_msgSend)(wifi_scanner,
        sel_registerName("start"));
    BOOL after = ((BOOL (*)(id, SEL))objc_msgSend)(wifi_scanner,
        sel_registerName("isScanning"));
    snprintf(line, sizeof(line), "scanner=%p after=%d", wifi_scanner,
        after ? 1 : 0);
    debug_line("request-scan-after", line);
    if (bulk_target) {
        Class timer_class = objc_getClass("NSTimer");
        ((id (*)(id, SEL, double, id, SEL, id, BOOL))objc_msgSend)(
            timer_class,
            sel_registerName("scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:"),
            45.0, bulk_target, sel_registerName("scanTimeout:"), 0, 0);
    }
    update_dashboard();
    return 1;
}

static void scan_timeout(id self, SEL cmd, id timer) {
    (void)self; (void)cmd; (void)timer;
    if (!scan_in_progress || !scan_requested_at ||
        time(0) - scan_requested_at < 44) return;
    scan_in_progress = 0;
    scan_timed_out = 1;
    bulk_confirm_pending = 0;
    bulk_confirm_count = 0;
    if (refresh_control)
        ((void (*)(id, SEL))objc_msgSend)(refresh_control,
            sel_registerName("endRefreshing"));
    update_dashboard();
    debug_line("wifi-scan", "timed out");
}

static void bulk_button_tapped(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    if (bulk_count) {
        for (int i = 0; i < bulk_count; ++i)
            stop_block_for_ip(bulk_ips[i]);
        bulk_count = 0;
        bulk_confirm_count = 0;
        bulk_last_failed = 0;
        update_dashboard();
        debug_line("bulk-action", "unblocked");
        return;
    }
    time_t now = time(0);
    if (scan_in_progress) {
        bulk_confirm_pending = 1;
        update_dashboard();
        return;
    }
    if (!bulk_confirm_count || now > bulk_confirm_until) {
        bulk_confirm_count = 0;
        bulk_confirm_pending = 1;
        if (!request_wifi_scan()) {
            bulk_confirm_pending = 0;
            scan_timed_out = 1;
            update_dashboard();
        }
        return;
    }
    struct scanned_device candidates[64];
    int count = bulk_eligible(candidates, 64);
    int same = count == confirmed_count;
    for (int i = 0; same && i < count; ++i)
        if (!equals(candidates[i].ip, confirmed_devices[i].ip) ||
            !equals(candidates[i].mac, confirmed_devices[i].mac)) same = 0;
    if (!same || !count) {
        bulk_confirm_count = 0;
        bulk_confirm_pending = 1;
        if (!request_wifi_scan()) {
            bulk_confirm_pending = 0;
            scan_timed_out = 1;
            update_dashboard();
        }
        return;
    }
    bulk_confirm_count = 0;
    bulk_last_failed = 0;
    Class commands = objc_getClass("_TtC13HarpyReloaded10MCCommands");
    if (!commands || !class_getClassMethod(commands,
        sel_registerName("blockGivenIPWithIp:targetMac:"))) {
        scan_timed_out = 1;
        update_dashboard();
        return;
    }
    for (int i = 0; i < count; ++i) {
        int already_blocked = 0;
        for (int j = 0; j < 64; ++j)
            if (blocks[j].pid > 0 && equals(blocks[j].ip, candidates[i].ip))
                already_blocked = 1;
        if (already_blocked) continue;
        ((void (*)(id, SEL, id, id))objc_msgSend)(commands,
            sel_registerName("blockGivenIPWithIp:targetMac:"),
            string_from_utf8(candidates[i].ip), string_from_utf8(candidates[i].mac));
        int launched = 0;
        for (int j = 0; j < 64; ++j)
            if (equals(blocks[j].ip, candidates[i].ip) &&
                pid_is_alive(blocks[j].pid)) launched = 1;
        if (launched && bulk_count < 64) {
            snprintf(bulk_ips[bulk_count], sizeof(bulk_ips[bulk_count]), "%s", candidates[i].ip);
            ++bulk_count;
        } else ++bulk_last_failed;
    }
    update_dashboard();
    debug_line("bulk-action", bulk_count ? "blocked" : "nothing to block");
}

static void refresh_triggered(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    bulk_confirm_count = 0;
    bulk_confirm_pending = 0;
    if (!request_wifi_scan()) {
        scan_timed_out = 1;
        if (refresh_control)
            ((void (*)(id, SEL))objc_msgSend)(refresh_control,
                sel_registerName("endRefreshing"));
        update_dashboard();
    }
}

static void show_alias_editor(id self, SEL cmd, id unused) {
    (void)self; (void)cmd; (void)unused;
    if (!presenting_controller || !alias_selected_mac[0]) return;
    Class alert_class = objc_getClass("UIAlertController");
    Class action_class = objc_getClass("UIAlertAction");
    char title[96];
    snprintf(title, sizeof(title), "Nombre de %s", alias_selected_ip);
    alias_alert = ((id (*)(id, SEL, id, id, long))objc_msgSend)(alert_class,
        sel_registerName("alertControllerWithTitle:message:preferredStyle:"),
        string_from_utf8(title),
        string_from_utf8("El nombre se guarda para esta dirección MAC."), 1);
    void (^configure)(id) = ^(id field) {
        ((void (*)(id, SEL, id))objc_msgSend)(field,
            sel_registerName("setPlaceholder:"), string_from_utf8("Nombre del equipo"));
        id alias = alias_for_mac(alias_selected_mac);
        if (alias) ((void (*)(id, SEL, id))objc_msgSend)(field,
            sel_registerName("setText:"), alias);
    };
    ((void (*)(id, SEL, void (^)(id)))objc_msgSend)(alias_alert,
        sel_registerName("addTextFieldWithConfigurationHandler:"), configure);
    void (^save)(id) = ^(id action) {
        (void)action;
        id fields = ((id (*)(id, SEL))objc_msgSend)(alias_alert,
            sel_registerName("textFields"));
        id field = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(fields,
            sel_registerName("objectAtIndex:"), 0);
        id value = ((id (*)(id, SEL))objc_msgSend)(field,
            sel_registerName("text"));
        Class chars = objc_getClass("NSCharacterSet");
        id whitespace = ((id (*)(id, SEL))objc_msgSend)(chars,
            sel_registerName("whitespaceAndNewlineCharacterSet"));
        id trimmed = ((id (*)(id, SEL, id))objc_msgSend)(value,
            sel_registerName("stringByTrimmingCharactersInSet:"), whitespace);
        save_alias_for_mac(alias_selected_mac, trimmed);
        alias_alert = 0;
        request_wifi_scan();
    };
    void (^remove)(id) = ^(id action) {
        (void)action;
        save_alias_for_mac(alias_selected_mac, 0);
        alias_alert = 0;
        request_wifi_scan();
    };
    id save_action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8("Guardar"), 0, save);
    id remove_action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8("Quitar nombre"), 2, remove);
    id cancel_action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8("Cancelar"), 1, (void (^)(id))0);
    ((void (*)(id, SEL, id))objc_msgSend)(alias_alert,
        sel_registerName("addAction:"), save_action);
    ((void (*)(id, SEL, id))objc_msgSend)(alias_alert,
        sel_registerName("addAction:"), remove_action);
    ((void (*)(id, SEL, id))objc_msgSend)(alias_alert,
        sel_registerName("addAction:"), cancel_action);
    ((void (*)(id, SEL, id, BOOL, id))objc_msgSend)(presenting_controller,
        sel_registerName("presentViewController:animated:completion:"),
        alias_alert, 1, 0);
}

static void show_names_tapped(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    if (!presenting_controller || !scanned_count) return;
    Class alert_class = objc_getClass("UIAlertController");
    Class action_class = objc_getClass("UIAlertAction");
    id picker = ((id (*)(id, SEL, id, id, long))objc_msgSend)(alert_class,
        sel_registerName("alertControllerWithTitle:message:preferredStyle:"),
        string_from_utf8("Nombres de equipos"),
        string_from_utf8("Elige un equipo para ponerle un nombre."), 0);
    void (^selected)(id) = ^(id action) {
        const char *title = utf8(((id (*)(id, SEL))objc_msgSend)(action,
            sel_registerName("title")));
        char ip[32] = {0};
        int n = 0;
        while (title && title[n] && title[n] != ' ' && n < 31) {
            ip[n] = title[n];
            ++n;
        }
        for (int i = 0; i < scanned_count; ++i) {
            if (equals(scanned_devices[i].ip, ip)) {
                snprintf(alias_selected_mac, sizeof(alias_selected_mac), "%s",
                    scanned_devices[i].mac);
                snprintf(alias_selected_ip, sizeof(alias_selected_ip), "%s", ip);
                ((void (*)(id, SEL, SEL, id, double))objc_msgSend)(bulk_target,
                    sel_registerName("performSelector:withObject:afterDelay:"),
                    sel_registerName("showAliasEditor:"), 0, 0.3);
                break;
            }
        }
    };
    for (int i = 0; i < scanned_count; ++i) {
        char line[160];
        snprintf(line, sizeof(line), "%s  ·  %s", scanned_devices[i].ip,
            scanned_devices[i].name[0] ? scanned_devices[i].name : "Equipo");
        id action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
            action_class, sel_registerName("actionWithTitle:style:handler:"),
            string_from_utf8(line), 0, selected);
        ((void (*)(id, SEL, id))objc_msgSend)(picker,
            sel_registerName("addAction:"), action);
    }
    id cancel = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8("Cancelar"), 1, (void (^)(id))0);
    ((void (*)(id, SEL, id))objc_msgSend)(picker,
        sel_registerName("addAction:"), cancel);
    ((void (*)(id, SEL, id, BOOL, id))objc_msgSend)(presenting_controller,
        sel_registerName("presentViewController:animated:completion:"), picker,
        1, 0);
}

static void show_status_tapped(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    if (!presenting_controller) return;
    Class alert_class = objc_getClass("UIAlertController");
    Class action_class = objc_getClass("UIAlertAction");
    id sheet = ((id (*)(id, SEL, id, id, long))objc_msgSend)(alert_class,
        sel_registerName("alertControllerWithTitle:message:preferredStyle:"),
        string_from_utf8("Estado de los equipos"),
        string_from_utf8("Activo indica que el proceso de bloqueo sigue ejecutándose."), 0);
    char local_ip[32] = {0}, router_ip[32] = {0}, router_mac[32] = {0};
    get_interface_ip("en0", local_ip, sizeof(local_ip));
    get_gateway(router_ip, sizeof(router_ip), router_mac, sizeof(router_mac));
    for (int i = 0; i < scanned_count; ++i) {
        struct scanned_device *device = &scanned_devices[i];
        int active = 0;
        for (int j = 0; j < 64; ++j)
            if (equals(blocks[j].ip, device->ip) && pid_is_alive(blocks[j].pid))
                active = 1;
        const char *state = equals(device->ip, local_ip) ||
            equals(device->ip, router_ip) ? "Excluido" :
            active ? "Activo" : "Sin bloqueo";
        char line[160];
        snprintf(line, sizeof(line), "%s · %s", device->ip, state);
        id action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
            action_class, sel_registerName("actionWithTitle:style:handler:"),
            string_from_utf8(line), 0, (void (^)(id))0);
        ((void (*)(id, SEL, id))objc_msgSend)(sheet,
            sel_registerName("addAction:"), action);
    }
    id cancel = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8("Cerrar"), 1, (void (^)(id))0);
    ((void (*)(id, SEL, id))objc_msgSend)(sheet,
        sel_registerName("addAction:"), cancel);
    ((void (*)(id, SEL, id, BOOL, id))objc_msgSend)(presenting_controller,
        sel_registerName("presentViewController:animated:completion:"), sheet,
        1, 0);
}

static id find_wifi_scroll(id view, int depth, double *best_area) {
    if (!view || depth > 18) return 0;
    id result = 0;
    BOOL is_scroll = ((BOOL (*)(id, SEL, id))objc_msgSend)(view,
        sel_registerName("isKindOfClass:"), objc_getClass("UIScrollView"));
    if (is_scroll) {
        struct cg_rect frame = ((struct cg_rect (*)(id, SEL))objc_msgSend)(
            view, sel_registerName("frame"));
        double area = frame.size.width * frame.size.height;
        if (frame.size.width > 240 && frame.size.height > 250 &&
            area > *best_area) {
            *best_area = area;
            result = view;
        }
    }
    id children = ((id (*)(id, SEL))objc_msgSend)(view,
        sel_registerName("subviews"));
    NSUInteger count = children ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        children, sel_registerName("count")) : 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id child = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(children,
            sel_registerName("objectAtIndex:"), i);
        id found = find_wifi_scroll(child, depth + 1, best_area);
        if (found) result = found;
    }
    return result;
}

static void attach_refresh_control(id root) {
    double best_area = 0;
    id scroll = find_wifi_scroll(root, 0, &best_area);
    if (!scroll) { debug_line("pull-refresh", "scroll unavailable"); return; }
    id existing = ((id (*)(id, SEL))objc_msgSend)(scroll,
        sel_registerName("refreshControl"));
    if (existing) { refresh_control = existing; return; }
    Class refresh_class = objc_getClass("UIRefreshControl");
    id refresh = ((id (*)(id, SEL))objc_msgSend)(refresh_class,
        sel_registerName("new"));
    Class color_class = objc_getClass("UIColor");
    id tint = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("systemRedColor"));
    ((void (*)(id, SEL, id))objc_msgSend)(refresh,
        sel_registerName("setTintColor:"), tint);
    Class attr_class = objc_getClass("NSAttributedString");
    id caption = ((id (*)(id, SEL))objc_msgSend)(attr_class,
        sel_registerName("alloc"));
    caption = ((id (*)(id, SEL, id))objc_msgSend)(caption,
        sel_registerName("initWithString:"),
        string_from_utf8(tr7("Actualizando equipos...", "Refreshing devices...",
            "Actualisation des appareils...", "Geräte werden aktualisiert...",
            "正在刷新设备...", "正在重新整理裝置...", "端末を更新中...")));
    ((void (*)(id, SEL, id))objc_msgSend)(refresh,
        sel_registerName("setAttributedTitle:"), caption);
    ((void (*)(id, SEL))objc_msgSend)(caption, sel_registerName("release"));
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(refresh,
        sel_registerName("addTarget:action:forControlEvents:"), bulk_target,
        sel_registerName("refreshTriggered:"), 1UL << 12);
    ((void (*)(id, SEL, id))objc_msgSend)(scroll,
        sel_registerName("setRefreshControl:"), refresh);
    refresh_control = refresh;
    ((void (*)(id, SEL))objc_msgSend)(refresh, sel_registerName("release"));
    debug_line("pull-refresh", "attached");
}

static void attach_bulk_button(id view) {
    if (!view) return;
    if (!bulk_target) {
        Class target_class = objc_allocateClassPair(objc_getClass("NSObject"),
            "NukeWirelessBulkButtonTarget", 0);
        if (!target_class) return;
        class_addMethod(target_class, sel_registerName("bulkButtonTapped:"),
            (IMP)bulk_button_tapped, "v@:@");
        class_addMethod(target_class, sel_registerName("refreshTriggered:"),
            (IMP)refresh_triggered, "v@:@");
        class_addMethod(target_class, sel_registerName("scanTimeout:"),
            (IMP)scan_timeout, "v@:@");
        class_addMethod(target_class, sel_registerName("showNames:"),
            (IMP)show_names_tapped, "v@:@");
        class_addMethod(target_class, sel_registerName("showAliasEditor:"),
            (IMP)show_alias_editor, "v@:@");
        class_addMethod(target_class, sel_registerName("showStatus:"),
            (IMP)show_status_tapped, "v@:@");
        objc_registerClassPair(target_class);
        bulk_target = ((id (*)(id, SEL))objc_msgSend)(target_class,
            sel_registerName("new"));
    }
    id previous = ((id (*)(id, SEL, long))objc_msgSend)(view,
        sel_registerName("viewWithTag:"), 90122);
    if (previous) {
        bulk_panel = previous;
        update_dashboard();
        id label = ((id (*)(id, SEL, long))objc_msgSend)(
            previous, sel_registerName("viewWithTag:"), 90130L);
        id block_button = ((id (*)(id, SEL, long))objc_msgSend)(
            previous, sel_registerName("viewWithTag:"), 90131L);
        id names_button = ((id (*)(id, SEL, long))objc_msgSend)(
            previous, sel_registerName("viewWithTag:"), 90132L);
        render_dashboard_controls(label, block_button, names_button);
        return;
    }
    Class view_class = objc_getClass("UIView");
    id panel = ((id (*)(id, SEL))objc_msgSend)(view_class,
        sel_registerName("alloc"));
    Class button_class = objc_getClass("UIButton");
    id button = ((id (*)(id, SEL, long))objc_msgSend)(button_class,
        sel_registerName("buttonWithType:"), 1);
    id name_button = ((id (*)(id, SEL, long))objc_msgSend)(button_class,
        sel_registerName("buttonWithType:"), 1);
    struct cg_rect bounds = ((struct cg_rect (*)(id, SEL))objc_msgSend)(view,
        sel_registerName("bounds"));
    struct cg_rect panel_frame = {{12, bounds.size.height - 198},
                                  {bounds.size.width - 24, 108}};
    panel = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(panel,
        sel_registerName("initWithFrame:"), panel_frame);
    ((void (*)(id, SEL, long))objc_msgSend)(panel,
        sel_registerName("setTag:"), 90122);
    Class color_class = objc_getClass("UIColor");
    id surface = ((id (*)(id, SEL, double, double))objc_msgSend)(color_class,
        sel_registerName("colorWithWhite:alpha:"), 0.13, 0.96);
    id red = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("systemRedColor"));
    id white = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("whiteColor"));
    id secondary = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("lightGrayColor"));
    id gray = ((id (*)(id, SEL, double, double))objc_msgSend)(color_class,
        sel_registerName("colorWithWhite:alpha:"), 0.28, 1.0);
    ((void (*)(id, SEL, id))objc_msgSend)(panel,
        sel_registerName("setBackgroundColor:"), surface);
    id layer = ((id (*)(id, SEL))objc_msgSend)(panel,
        sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(layer,
        sel_registerName("setCornerRadius:"), 14.0);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(layer,
        sel_registerName("setMasksToBounds:"), 1);
    Class label_class = objc_getClass("UILabel");
    id label = ((id (*)(id, SEL))objc_msgSend)(label_class,
        sel_registerName("alloc"));
    struct cg_rect label_frame = {{14, 8}, {panel_frame.size.width - 28, 48}};
    label = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(label,
        sel_registerName("initWithFrame:"), label_frame);
    ((void (*)(id, SEL, long))objc_msgSend)(label,
        sel_registerName("setTag:"), 90130L);
    ((void (*)(id, SEL, long))objc_msgSend)(label,
        sel_registerName("setNumberOfLines:"), 2);
    Class font_class = objc_getClass("UIFont");
    id font = ((id (*)(id, SEL, double))objc_msgSend)(font_class,
        sel_registerName("systemFontOfSize:"), 12.0);
    ((void (*)(id, SEL, id))objc_msgSend)(label,
        sel_registerName("setFont:"), font);
    ((void (*)(id, SEL, id))objc_msgSend)(label,
        sel_registerName("setTextColor:"), secondary);
    ((void (*)(id, SEL, id))objc_msgSend)(label,
        sel_registerName("setAccessibilityHint:"),
        string_from_utf8(tr7(
            "Toca para ver el estado de cada equipo",
            "Tap to view the status of each device",
            "Touchez pour voir l’état de chaque appareil",
            "Tippen, um den Status jedes Geräts anzuzeigen",
            "轻点以查看每台设备的状态",
            "點一下以查看每部裝置的狀態",
            "タップして各端末の状態を表示")));
    ((void (*)(id, SEL, BOOL))objc_msgSend)(label,
        sel_registerName("setUserInteractionEnabled:"), 1);
    Class tap_class = objc_getClass("UITapGestureRecognizer");
    id tap = ((id (*)(id, SEL))objc_msgSend)(tap_class,
        sel_registerName("alloc"));
    tap = ((id (*)(id, SEL, id, SEL))objc_msgSend)(tap,
        sel_registerName("initWithTarget:action:"), bulk_target,
        sel_registerName("showStatus:"));
    ((void (*)(id, SEL, id))objc_msgSend)(label,
        sel_registerName("addGestureRecognizer:"), tap);
    ((void (*)(id, SEL))objc_msgSend)(tap, sel_registerName("release"));
    ((void (*)(id, SEL, id))objc_msgSend)(panel,
        sel_registerName("addSubview:"), label);
    status_label = label;
    ((void (*)(id, SEL))objc_msgSend)(label, sel_registerName("release"));
    struct cg_rect frame = {{12, 57}, {panel_frame.size.width - 118, 40}};
    ((void (*)(id, SEL, struct cg_rect))objc_msgSend)(button,
        sel_registerName("setFrame:"), frame);
    ((void (*)(id, SEL, long))objc_msgSend)(button,
        sel_registerName("setTag:"), 90131L);
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(button,
        sel_registerName("addTarget:action:forControlEvents:"), bulk_target,
        sel_registerName("bulkButtonTapped:"), 1UL << 6);
    ((void (*)(id, SEL, id))objc_msgSend)(button,
        sel_registerName("setBackgroundColor:"), red);
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(button,
        sel_registerName("setTitleColor:forState:"), white, 0);
    id button_layer = ((id (*)(id, SEL))objc_msgSend)(button,
        sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(button_layer,
        sel_registerName("setCornerRadius:"), 9.0);
    struct cg_rect name_frame = {{panel_frame.size.width - 96, 57}, {84, 40}};
    ((void (*)(id, SEL, struct cg_rect))objc_msgSend)(name_button,
        sel_registerName("setFrame:"), name_frame);
    ((void (*)(id, SEL, long))objc_msgSend)(name_button,
        sel_registerName("setTag:"), 90132L);
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(name_button,
        sel_registerName("addTarget:action:forControlEvents:"), bulk_target,
        sel_registerName("showNames:"), 1UL << 6);
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(name_button,
        sel_registerName("setTitle:forState:"),
        string_from_utf8(tr7("Nombres", "Names", "Noms", "Namen",
            "名称", "名稱", "名前")), 0);
    ((void (*)(id, SEL, id))objc_msgSend)(name_button,
        sel_registerName("setBackgroundColor:"), gray);
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(name_button,
        sel_registerName("setTitleColor:forState:"), white, 0);
    id name_layer = ((id (*)(id, SEL))objc_msgSend)(name_button,
        sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(name_layer,
        sel_registerName("setCornerRadius:"), 9.0);
    ((void (*)(id, SEL, id))objc_msgSend)(panel,
        sel_registerName("addSubview:"), button);
    ((void (*)(id, SEL, id))objc_msgSend)(panel,
        sel_registerName("addSubview:"), name_button);
    ((void (*)(id, SEL, id))objc_msgSend)(view,
        sel_registerName("addSubview:"), panel);
    ((void (*)(id, SEL))objc_msgSend)(panel, sel_registerName("release"));
    bulk_panel = panel;
    bulk_button = button;
    alias_button = name_button;
    update_dashboard();
    debug_line("bulk-panel", "attached");
}

static void dump_view_tree(id view, int depth, int *remaining) {
    if (!view || depth > 20 || !*remaining) return;
    --*remaining;
    const char *class_name = class_getName(object_getClass(view));
    id label = ((id (*)(id, SEL))objc_msgSend)(view,
        sel_registerName("accessibilityLabel"));
    char line[512];
    snprintf(line, sizeof(line), "%d %s | %s", depth,
        class_name ? class_name : "?", utf8(label) ? utf8(label) : "");
    debug_line("view", line);
    if (contains(class_name, "HostingScrollView")) {
        struct cg_rect frame = ((struct cg_rect (*)(id, SEL))objc_msgSend)(
            view, sel_registerName("frame"));
        snprintf(line, sizeof(line), "x=%g y=%g w=%g h=%g",
            frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
        debug_line("scroll-frame", line);
        id elements = ((id (*)(id, SEL))objc_msgSend)(view,
            sel_registerName("accessibilityElements"));
        NSUInteger element_count = elements ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
            elements, sel_registerName("count")) : 0;
        for (NSUInteger i = 0; i < element_count && i < 80; ++i) {
            id element = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
                elements, sel_registerName("objectAtIndex:"), i);
            id element_label = ((id (*)(id, SEL))objc_msgSend)(element,
                sel_registerName("accessibilityLabel"));
            debug_line("scroll-accessibility", utf8(element_label));
        }
    }
    if (contains(class_name, "TableView")) {
        id data_source = ((id (*)(id, SEL))objc_msgSend)(view,
            sel_registerName("dataSource"));
        debug_line("table-data-source", data_source ? class_getName(object_getClass(data_source)) : "nil");
        NSUInteger sections = ((NSUInteger (*)(id, SEL))objc_msgSend)(view,
            sel_registerName("numberOfSections"));
        char count_text[40];
        snprintf(count_text, sizeof(count_text), "%lu", sections);
        debug_line("table-sections", count_text);
        for (NSUInteger i = 0; i < sections && i < 12; ++i) {
            NSUInteger rows = ((NSUInteger (*)(id, SEL, NSUInteger))objc_msgSend)(
                view, sel_registerName("numberOfRowsInSection:"), i);
            snprintf(count_text, sizeof(count_text), "%lu:%lu", i, rows);
            debug_line("table-rows", count_text);
        }
    }
    id children = ((id (*)(id, SEL))objc_msgSend)(view,
        sel_registerName("subviews"));
    NSUInteger count = children ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        children, sel_registerName("count")) : 0;
    for (NSUInteger i = 0; i < count && *remaining; ++i) {
        id child = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            children, sel_registerName("objectAtIndex:"), i);
        dump_view_tree(child, depth + 1, remaining);
    }
}

static int is_legacy_brand_text(const char *value) {
    return contains_case_insensitive(value, "harpy") ||
           contains_case_insensitive(value, "harrrrrpy") ||
           contains_case_insensitive(value, "harrrpy");
}

static void style_brand_label(id label, double size) {
    if (!label) return;
    Class label_class = object_getClass(label);
    if (class_getInstanceMethod(label_class, sel_registerName("setFont:"))) {
        Class font_class = objc_getClass("UIFont");
        id font = ((id (*)(id, SEL, double, double))objc_msgSend)(
            font_class, sel_registerName("systemFontOfSize:weight:"), size, 0.62);
        if (font) ((void (*)(id, SEL, id))objc_msgSend)(
            label, sel_registerName("setFont:"), font);
    }
    if (class_getInstanceMethod(label_class, sel_registerName("setTextColor:"))) {
        id red = ((id (*)(id, SEL))objc_msgSend)(
            objc_getClass("UIColor"), sel_registerName("systemRedColor"));
        ((void (*)(id, SEL, id))objc_msgSend)(
            label, sel_registerName("setTextColor:"), red);
    }
    if (class_getInstanceMethod(label_class, sel_registerName("setTextAlignment:")))
        ((void (*)(id, SEL, long))objc_msgSend)(
            label, sel_registerName("setTextAlignment:"), 1L);
}

static void rebrand_visible_view(id view, int depth) {
    if (!view || depth > 22) return;
    Class cls = object_getClass(view);
    SEL text_sel = sel_registerName("text");
    SEL set_text_sel = sel_registerName("setText:");
    if (class_getInstanceMethod(cls, text_sel) &&
        class_getInstanceMethod(cls, set_text_sel)) {
        id current = ((id (*)(id, SEL))objc_msgSend)(view, text_sel);
        id localized = localized_device_action_text(current);
        if (localized != current) {
            ((void (*)(id, SEL, id))objc_msgSend)(
                view, set_text_sel, localized);
        } else if (is_legacy_brand_text(utf8(current))) {
            ((void (*)(id, SEL, id))objc_msgSend)(
                view, set_text_sel, string_from_utf8("Nuke Wireless"));
            style_brand_label(view, 30.0);
        }
    }
    if (((BOOL (*)(id, SEL, id))objc_msgSend)(
            view, sel_registerName("isKindOfClass:"), objc_getClass("UIButton"))) {
        id title = ((id (*)(id, SEL))objc_msgSend)(
            view, sel_registerName("currentTitle"));
        id localized = localized_device_action_text(title);
        if (localized != title)
            ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
                view, sel_registerName("setTitle:forState:"), localized, 0);
        else if (is_legacy_brand_text(utf8(title)))
            ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
                view, sel_registerName("setTitle:forState:"),
                string_from_utf8("Nuke Wireless"), 0);
    }
    SEL accessibility_sel = sel_registerName("accessibilityLabel");
    SEL set_accessibility_sel = sel_registerName("setAccessibilityLabel:");
    if (class_getInstanceMethod(cls, accessibility_sel) &&
        class_getInstanceMethod(cls, set_accessibility_sel)) {
        id label = ((id (*)(id, SEL))objc_msgSend)(view, accessibility_sel);
        id localized = localized_device_action_text(label);
        if (localized != label)
            ((void (*)(id, SEL, id))objc_msgSend)(
                view, set_accessibility_sel, localized);
        else if (is_legacy_brand_text(utf8(label)))
            ((void (*)(id, SEL, id))objc_msgSend)(
                view, set_accessibility_sel, string_from_utf8("Nuke Wireless"));
    }
    id children = ((id (*)(id, SEL))objc_msgSend)(
        view, sel_registerName("subviews"));
    NSUInteger count = children ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        children, sel_registerName("count")) : 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id child = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            children, sel_registerName("objectAtIndex:"), i);
        rebrand_visible_view(child, depth + 1);
    }
}

static void hide_underlying_brand_text(id view, int depth) {
    if (!view || depth > 22) return;
    if (class_getInstanceMethod(object_getClass(view), sel_registerName("tag"))) {
        long tag = ((long (*)(id, SEL))objc_msgSend)(view, sel_registerName("tag"));
        if (tag == 90125L) return;
    }
    Class cls = object_getClass(view);
    if (class_getInstanceMethod(cls, sel_registerName("text"))) {
        id text = ((id (*)(id, SEL))objc_msgSend)(view, sel_registerName("text"));
        const char *value = utf8(text);
        if (is_legacy_brand_text(value) || equals(value, "Nuke Wireless")) {
            if (class_getInstanceMethod(cls, sel_registerName("setHidden:")))
                ((void (*)(id, SEL, BOOL))objc_msgSend)(
                    view, sel_registerName("setHidden:"), 1);
        }
    }
    id children = ((id (*)(id, SEL))objc_msgSend)(
        view, sel_registerName("subviews"));
    NSUInteger count = children ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        children, sel_registerName("count")) : 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id child = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            children, sel_registerName("objectAtIndex:"), i);
        hide_underlying_brand_text(child, depth + 1);
    }
}

static void rebrand_controller(id controller) {
    if (!controller) return;
    Class cls = object_getClass(controller);
    SEL title_sel = sel_registerName("title");
    SEL set_title_sel = sel_registerName("setTitle:");
    if (class_getInstanceMethod(cls, title_sel) &&
        class_getInstanceMethod(cls, set_title_sel)) {
        id title = ((id (*)(id, SEL))objc_msgSend)(controller, title_sel);
        if (is_legacy_brand_text(utf8(title)))
            ((void (*)(id, SEL, id))objc_msgSend)(
                controller, set_title_sel, string_from_utf8("Nuke Wireless"));
    }
    id navigation_item = ((id (*)(id, SEL))objc_msgSend)(
        controller, sel_registerName("navigationItem"));
    if (navigation_item) {
        id title = ((id (*)(id, SEL))objc_msgSend)(
            navigation_item, sel_registerName("title"));
        if (is_legacy_brand_text(utf8(title)))
            ((void (*)(id, SEL, id))objc_msgSend)(
                navigation_item, sel_registerName("setTitle:"),
                string_from_utf8("Nuke Wireless"));
    }
    id tab_item = ((id (*)(id, SEL))objc_msgSend)(
        controller, sel_registerName("tabBarItem"));
    if (tab_item) {
        id title = ((id (*)(id, SEL))objc_msgSend)(
            tab_item, sel_registerName("title"));
        if (is_legacy_brand_text(utf8(title)))
            ((void (*)(id, SEL, id))objc_msgSend)(
                tab_item, sel_registerName("setTitle:"),
                string_from_utf8("Nuke Wireless"));
    }
    id view = ((id (*)(id, SEL))objc_msgSend)(
        controller, sel_registerName("view"));
    rebrand_visible_view(view, 0);
}

static void attach_brand_header(id root) {
    if (!root) return;
    id previous = ((id (*)(id, SEL, long))objc_msgSend)(
        root, sel_registerName("viewWithTag:"), 90125L);
    if (previous) {
        ((void (*)(id, SEL, BOOL))objc_msgSend)(
            previous, sel_registerName("setHidden:"), 0);
        ((void (*)(id, SEL, id))objc_msgSend)(
            root, sel_registerName("bringSubviewToFront:"), previous);
        return;
    }

    struct cg_rect bounds = ((struct cg_rect (*)(id, SEL))objc_msgSend)(
        root, sel_registerName("bounds"));
    struct ui_edge_insets safe = {0, 0, 0, 0};
    if (class_getInstanceMethod(object_getClass(root), sel_registerName("safeAreaInsets")))
        safe = ((struct ui_edge_insets (*)(id, SEL))objc_msgSend)(
            root, sel_registerName("safeAreaInsets"));

    double width = bounds.size.width > 60 ? bounds.size.width - 28 : bounds.size.width;
    double x = bounds.size.width > 60 ? 14.0 : 0.0;
    double y = safe.top + 6.0;
    struct cg_rect card_frame = {{x, y}, {width, 62.0}};

    Class view_class = objc_getClass("UIView");
    Class color_class = objc_getClass("UIColor");
    id card = ((id (*)(id, SEL))objc_msgSend)(
        view_class, sel_registerName("alloc"));
    card = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(
        card, sel_registerName("initWithFrame:"), card_frame);
    ((void (*)(id, SEL, long))objc_msgSend)(
        card, sel_registerName("setTag:"), 90125L);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        card, sel_registerName("setUserInteractionEnabled:"), 0);

    id surface = ((id (*)(id, SEL, double, double))objc_msgSend)(
        color_class, sel_registerName("colorWithWhite:alpha:"), 0.055, 0.97);
    id red = ((id (*)(id, SEL))objc_msgSend)(
        color_class, sel_registerName("systemRedColor"));
    id secondary = ((id (*)(id, SEL, double, double))objc_msgSend)(
        color_class, sel_registerName("colorWithWhite:alpha:"), 0.72, 1.0);
    ((void (*)(id, SEL, id))objc_msgSend)(
        card, sel_registerName("setBackgroundColor:"), surface);

    id layer = ((id (*)(id, SEL))objc_msgSend)(card, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        layer, sel_registerName("setCornerRadius:"), 16.0);
    ((void (*)(id, SEL, double))objc_msgSend)(
        layer, sel_registerName("setBorderWidth:"), 1.0);
    id red_cg = ((id (*)(id, SEL))objc_msgSend)(red, sel_registerName("CGColor"));
    ((void (*)(id, SEL, id))objc_msgSend)(
        layer, sel_registerName("setBorderColor:"), red_cg);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        layer, sel_registerName("setMasksToBounds:"), 1);

    add_info_label(card, (struct cg_rect){{14, 7}, {width - 28, 31}},
        "Nuke Wireless", 22.0, 0.75, red, 1L, 1L);
    add_info_label(card, (struct cg_rect){{14, 35}, {width - 28, 18}},
        "NETWORK KILLER", 10.5, 0.35, secondary, 1L, 1L);

    ((void (*)(id, SEL, id))objc_msgSend)(
        root, sel_registerName("addSubview:"), card);
    ((void (*)(id, SEL, id))objc_msgSend)(
        root, sel_registerName("bringSubviewToFront:"), card);
    ((void (*)(id, SEL))objc_msgSend)(card, sel_registerName("release"));
    debug_line("brand-header", "attached");
}

static id content_controller_for(id controller) {
    if (!controller) return 0;
    Class cls = object_getClass(controller);
    if (class_getInstanceMethod(cls, sel_registerName("visibleViewController"))) {
        id visible = ((id (*)(id, SEL))objc_msgSend)(
            controller, sel_registerName("visibleViewController"));
        if (visible) return visible;
    }
    if (class_getInstanceMethod(cls, sel_registerName("topViewController"))) {
        id top = ((id (*)(id, SEL))objc_msgSend)(
            controller, sel_registerName("topViewController"));
        if (top) return top;
    }
    return controller;
}

static void localize_tab_items(id tab) {
    if (!tab) return;
    id controllers = ((id (*)(id, SEL))objc_msgSend)(
        tab, sel_registerName("viewControllers"));
    NSUInteger count = controllers ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        controllers, sel_registerName("count")) : 0;
    const char *titles[3] = {
        tr7("Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi"),
        tr7("Punto de acceso", "Hotspot", "Point d’accès", "Hotspot",
            "热点", "熱點", "ホットスポット"),
        tr7("Info", "Info", "Infos", "Info", "信息", "資訊", "情報")
    };
    for (NSUInteger i = 0; i < count && i < 3; ++i) {
        id controller = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            controllers, sel_registerName("objectAtIndex:"), i);
        id item = ((id (*)(id, SEL))objc_msgSend)(
            controller, sel_registerName("tabBarItem"));
        if (item)
            ((void (*)(id, SEL, id))objc_msgSend)(
                item, sel_registerName("setTitle:"), string_from_utf8(titles[i]));
    }
}

static void localize_controller_title(id controller, NSUInteger index) {
    if (!controller || index > 2) return;
    const char *titles[3] = {
        tr7("Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi", "Wi-Fi"),
        tr7("Punto de acceso", "Hotspot", "Point d’accès", "Hotspot",
            "热点", "熱點", "ホットスポット"),
        tr7("Info", "Info", "Infos", "Info", "信息", "資訊", "情報")
    };
    id title = string_from_utf8(titles[index]);
    if (class_getInstanceMethod(object_getClass(controller), sel_registerName("setTitle:")))
        ((void (*)(id, SEL, id))objc_msgSend)(
            controller, sel_registerName("setTitle:"), title);
    id navigation_item = ((id (*)(id, SEL))objc_msgSend)(
        controller, sel_registerName("navigationItem"));
    if (navigation_item)
        ((void (*)(id, SEL, id))objc_msgSend)(
            navigation_item, sel_registerName("setTitle:"), title);
}

static void refresh_language_ui_after_picker(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    if (!info_presenting_controller) return;
    id tab = ((id (*)(id, SEL))objc_msgSend)(
        info_presenting_controller, sel_registerName("tabBarController"));
    if (!tab) return;
    localize_tab_items(tab);
    NSUInteger selected = ((NSUInteger (*)(id, SEL))objc_msgSend)(
        tab, sel_registerName("selectedIndex"));
    id selected_controller = ((id (*)(id, SEL))objc_msgSend)(
        tab, sel_registerName("selectedViewController"));
    id content_controller = content_controller_for(selected_controller);
    localize_controller_title(content_controller, selected);
    if (selected == 2 && content_controller) {
        id view = ((id (*)(id, SEL))objc_msgSend)(
            content_controller, sel_registerName("view"));
        render_info_screen(view);
    }
}

static void show_language_picker(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    if (!info_presenting_controller) return;
    Class alert_class = objc_getClass("UIAlertController");
    Class action_class = objc_getClass("UIAlertAction");
    id picker = ((id (*)(id, SEL, id, id, long))objc_msgSend)(
        alert_class, sel_registerName("alertControllerWithTitle:message:preferredStyle:"),
        string_from_utf8(tr7(
            "Idioma", "Language", "Langue", "Sprache",
            "语言", "語言", "言語")),
        string_from_utf8(tr7(
            "Elige el idioma de Nuke Wireless.",
            "Choose the language used by Nuke Wireless.",
            "Choisissez la langue de Nuke Wireless.",
            "Wähle die Sprache von Nuke Wireless.",
            "选择 Nuke Wireless 的语言。",
            "選擇 Nuke Wireless 的語言。",
            "Nuke Wireless で使用する言語を選択します。")), 0);

    for (int raw = NW_LANG_ES; raw <= NW_LANG_JA; ++raw) {
        enum nw_language language = (enum nw_language)raw;
        void (^selected)(id) = ^(id action) {
            (void)action;
            set_language(language);
            if (info_target)
                ((void (*)(id, SEL, SEL, id, double))objc_msgSend)(
                    info_target,
                    sel_registerName("performSelector:withObject:afterDelay:"),
                    sel_registerName("refreshLanguageUI:"), 0, 0.45);
        };
        id action = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
            action_class, sel_registerName("actionWithTitle:style:handler:"),
            string_from_utf8(language_name(language)), 0, selected);
        ((void (*)(id, SEL, id))objc_msgSend)(
            picker, sel_registerName("addAction:"), action);
    }
    id cancel = ((id (*)(id, SEL, id, long, void (^)(id)))objc_msgSend)(
        action_class, sel_registerName("actionWithTitle:style:handler:"),
        string_from_utf8(tr7(
            "Cancelar", "Cancel", "Annuler", "Abbrechen",
            "取消", "取消", "キャンセル")), 1, (void (^)(id))0);
    ((void (*)(id, SEL, id))objc_msgSend)(
        picker, sel_registerName("addAction:"), cancel);
    ((void (*)(id, SEL, id, BOOL, id))objc_msgSend)(
        info_presenting_controller,
        sel_registerName("presentViewController:animated:completion:"),
        picker, 1, 0);
}

static void open_external_url(const char *value) {
    id url = ((id (*)(id, SEL, id))objc_msgSend)(
        objc_getClass("NSURL"), sel_registerName("URLWithString:"),
        string_from_utf8(value));
    if (!url) return;
    id app = ((id (*)(id, SEL))objc_msgSend)(
        objc_getClass("UIApplication"), sel_registerName("sharedApplication"));
    if (!app) return;
    SEL modern = sel_registerName("openURL:options:completionHandler:");
    if (class_getInstanceMethod(object_getClass(app), modern)) {
        id options = ((id (*)(id, SEL))objc_msgSend)(
            objc_getClass("NSDictionary"), sel_registerName("dictionary"));
        ((void (*)(id, SEL, id, id, id))objc_msgSend)(
            app, modern, url, options, 0);
    } else {
        ((BOOL (*)(id, SEL, id))objc_msgSend)(
            app, sel_registerName("openURL:"), url);
    }
}

static void open_repository(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    open_external_url("https://github.com/Gokuencinar/GokuEnREPO");
}

static void open_buy_me_a_coffee(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    open_external_url("https://buymeacoffee.com/gokuen");
}

static void ensure_info_target(void) {
    if (info_target) return;
    Class target_class = objc_allocateClassPair(
        objc_getClass("NSObject"), "NukeWirelessInfoTarget", 0);
    if (!target_class) target_class = objc_getClass("NukeWirelessInfoTarget");
    if (!target_class) return;
    class_addMethod(target_class, sel_registerName("showLanguagePicker:"),
        (IMP)show_language_picker, "v@:@");
    class_addMethod(target_class, sel_registerName("refreshLanguageUI:"),
        (IMP)refresh_language_ui_after_picker, "v@:@");
    class_addMethod(target_class, sel_registerName("openRepository:"),
        (IMP)open_repository, "v@:@");
    class_addMethod(target_class, sel_registerName("openBuyMeACoffee:"),
        (IMP)open_buy_me_a_coffee, "v@:@");
    if (!objc_getClass("NukeWirelessInfoTarget"))
        objc_registerClassPair(target_class);
    info_target = ((id (*)(id, SEL))objc_msgSend)(
        target_class, sel_registerName("new"));
}

static id find_info_scroll(id view, int depth) {
    if (!view || depth > 16) return 0;
    const char *name = class_getName(object_getClass(view));
    if (contains(name, "HostingScrollView")) {
        struct cg_rect frame = ((struct cg_rect (*)(id, SEL))objc_msgSend)(
            view, sel_registerName("frame"));
        if (frame.size.width > 300 && frame.size.height > 700 &&
            frame.size.height < 770) return view;
    }
    id children = ((id (*)(id, SEL))objc_msgSend)(view,
        sel_registerName("subviews"));
    NSUInteger count = children ? ((NSUInteger (*)(id, SEL))objc_msgSend)(
        children, sel_registerName("count")) : 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id child = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            children, sel_registerName("objectAtIndex:"), i);
        id result = find_info_scroll(child, depth + 1);
        if (result) return result;
    }
    return 0;
}

static id add_info_label(id parent, struct cg_rect frame, const char *text,
                         double size, double weight, id color,
                         long alignment, long lines) {
    Class label_class = objc_getClass("UILabel");
    id label = ((id (*)(id, SEL))objc_msgSend)(
        label_class, sel_registerName("alloc"));
    label = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(
        label, sel_registerName("initWithFrame:"), frame);
    ((void (*)(id, SEL, id))objc_msgSend)(
        label, sel_registerName("setText:"), string_from_utf8(text));
    ((void (*)(id, SEL, long))objc_msgSend)(
        label, sel_registerName("setNumberOfLines:"), lines);
    ((void (*)(id, SEL, long))objc_msgSend)(
        label, sel_registerName("setTextAlignment:"), alignment);
    ((void (*)(id, SEL, id))objc_msgSend)(
        label, sel_registerName("setTextColor:"), color);
    id font = ((id (*)(id, SEL, double, double))objc_msgSend)(
        objc_getClass("UIFont"), sel_registerName("systemFontOfSize:weight:"),
        size, weight);
    if (font) ((void (*)(id, SEL, id))objc_msgSend)(
        label, sel_registerName("setFont:"), font);
    ((void (*)(id, SEL, id))objc_msgSend)(
        parent, sel_registerName("addSubview:"), label);
    ((void (*)(id, SEL))objc_msgSend)(label, sel_registerName("release"));
    return label;
}

static void get_display_version(char *buffer, unsigned long buffer_size) {
    if (!buffer || !buffer_size) return;
    buffer[0] = 0;
    id bundle = ((id (*)(id, SEL))objc_msgSend)(
        objc_getClass("NSBundle"), sel_registerName("mainBundle"));
    if (!bundle) return;
    id short_version = ((id (*)(id, SEL, id))objc_msgSend)(
        bundle, sel_registerName("objectForInfoDictionaryKey:"),
        string_from_utf8("CFBundleShortVersionString"));
    id build_version = ((id (*)(id, SEL, id))objc_msgSend)(
        bundle, sel_registerName("objectForInfoDictionaryKey:"),
        string_from_utf8("CFBundleVersion"));
    const char *short_text = utf8(short_version);
    const char *build_text = utf8(build_version);
    if (short_text && short_text[0] && build_text && build_text[0] &&
        !equals(short_text, build_text))
        snprintf(buffer, buffer_size, "%s (%s)", short_text, build_text);
    else if (short_text && short_text[0])
        snprintf(buffer, buffer_size, "%s", short_text);
    else if (build_text && build_text[0])
        snprintf(buffer, buffer_size, "%s", build_text);
}

static void render_info_screen(id root) {
    if (!root) return;
    info_root_view = root;
    ensure_info_target();
    id previous = ((id (*)(id, SEL, long))objc_msgSend)(
        root, sel_registerName("viewWithTag:"), 90124L);
    if (previous)
        ((void (*)(id, SEL))objc_msgSend)(
            previous, sel_registerName("removeFromSuperview"));
    struct cg_rect bounds = ((struct cg_rect (*)(id, SEL))objc_msgSend)(
        root, sel_registerName("bounds"));
    double width = bounds.size.width > 0 ? bounds.size.width : 390.0;
    double height = bounds.size.height > 0 ? bounds.size.height : 780.0;
    double content_height = 1300.0;
    Class view_class = objc_getClass("UIView");
    Class scroll_class = objc_getClass("UIScrollView");
    id scroll = ((id (*)(id, SEL))objc_msgSend)(scroll_class,
        sel_registerName("alloc"));
    scroll = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(
        scroll, sel_registerName("initWithFrame:"),
        (struct cg_rect){{0, 0}, {width, height}});
    Class color_class = objc_getClass("UIColor");
    id black = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("blackColor"));
    id white = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("whiteColor"));
    id secondary = ((id (*)(id, SEL, double, double))objc_msgSend)(
        color_class, sel_registerName("colorWithWhite:alpha:"), 0.72, 1.0);
    id red = ((id (*)(id, SEL))objc_msgSend)(color_class,
        sel_registerName("systemRedColor"));
    ((void (*)(id, SEL, long))objc_msgSend)(
        scroll, sel_registerName("setTag:"), 90124L);
    ((void (*)(id, SEL, id))objc_msgSend)(scroll,
        sel_registerName("setBackgroundColor:"), black);
    struct cg_size content_size = {width, content_height};
    ((void (*)(id, SEL, struct cg_size))objc_msgSend)(
        scroll, sel_registerName("setContentSize:"), content_size);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        scroll, sel_registerName("setAlwaysBounceVertical:"), 1);
    ((void (*)(id, SEL, id))objc_msgSend)(
        root, sel_registerName("addSubview:"), scroll);

    struct cg_rect avatar_frame = {{(width - 78) / 2, 20}, {78, 78}};
    Class image_view_class = objc_getClass("UIImageView");
    id bundle = ((id (*)(id, SEL))objc_msgSend)(objc_getClass("NSBundle"),
        sel_registerName("mainBundle"));
    id avatar_path = ((id (*)(id, SEL, id, id))objc_msgSend)(bundle,
        sel_registerName("pathForResource:ofType:"),
        string_from_utf8("CreditsAvatar"), string_from_utf8("jpg"));
    id avatar_image = ((id (*)(id, SEL, id))objc_msgSend)(objc_getClass("UIImage"),
        sel_registerName("imageWithContentsOfFile:"), avatar_path);
    id avatar = ((id (*)(id, SEL))objc_msgSend)(image_view_class, sel_registerName("alloc"));
    avatar = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(avatar,
        sel_registerName("initWithFrame:"), avatar_frame);
    ((void (*)(id, SEL, id))objc_msgSend)(avatar, sel_registerName("setImage:"), avatar_image);
    ((void (*)(id, SEL, long))objc_msgSend)(avatar, sel_registerName("setContentMode:"), 2L);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(avatar, sel_registerName("setClipsToBounds:"), 1);
    id avatar_layer = ((id (*)(id, SEL))objc_msgSend)(avatar, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(avatar_layer,
        sel_registerName("setCornerRadius:"), 44.0);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(avatar_layer,
        sel_registerName("setMasksToBounds:"), 1);
    ((void (*)(id, SEL, id))objc_msgSend)(scroll, sel_registerName("addSubview:"), avatar);
    ((void (*)(id, SEL))objc_msgSend)(avatar, sel_registerName("release"));

    add_info_label(scroll, (struct cg_rect){{16, 108}, {width - 32, 38}},
        "Nuke Wireless", 28.0, 0.75, red, 1L, 1L);
    add_info_label(scroll, (struct cg_rect){{16, 145}, {width - 32, 22}},
        "Wi-Fi toolkit · RootHide · iOS 16", 13.0, 0.15, secondary, 1L, 1L);
    add_info_label(scroll, (struct cg_rect){{16, 170}, {width - 32, 22}},
        "Gokuencinar · GokuEn", 14.0, 0.35, white, 1L, 1L);
    char raw_version[96] = {0};
    char version_text[160];
    get_display_version(raw_version, sizeof(raw_version));
    snprintf(version_text, sizeof(version_text), "%s: %s",
        tr7("Versión", "Version", "Version", "Version",
            "版本", "版本", "バージョン"),
        raw_version[0] ? raw_version : NUKE_WIRELESS_RELEASE_VERSION);
    add_info_label(
        scroll, (struct cg_rect){{16, 189}, {width - 32, 24}},
        version_text, 14.0, 0.45, white, 1L, 1L);

    Class button_class = objc_getClass("UIButton");
    id language_button = ((id (*)(id, SEL, long))objc_msgSend)(
        button_class, sel_registerName("buttonWithType:"), 1);
    ((void (*)(id, SEL, struct cg_rect))objc_msgSend)(
        language_button, sel_registerName("setFrame:"),
        (struct cg_rect){{width - 134, 218}, {118, 34}});
    char language_title[96];
    snprintf(language_title, sizeof(language_title), "🌐 %s",
        language_name(current_language()));
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        language_button, sel_registerName("setTitle:forState:"),
        string_from_utf8(language_title), 0);
    ((void (*)(id, SEL, id))objc_msgSend)(
        language_button, sel_registerName("setBackgroundColor:"),
        ((id (*)(id, SEL, double, double))objc_msgSend)(
            color_class, sel_registerName("colorWithWhite:alpha:"), 0.16, 1.0));
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        language_button, sel_registerName("setTitleColor:forState:"), white, 0);
    id language_layer = ((id (*)(id, SEL))objc_msgSend)(
        language_button, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        language_layer, sel_registerName("setCornerRadius:"), 10.0);
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(
        language_button, sel_registerName("addTarget:action:forControlEvents:"),
        info_target, sel_registerName("showLanguagePicker:"), 1UL << 6);
    ((void (*)(id, SEL, id))objc_msgSend)(
        scroll, sel_registerName("addSubview:"), language_button);

    id repo_button = ((id (*)(id, SEL, long))objc_msgSend)(
        button_class, sel_registerName("buttonWithType:"), 1);
    ((void (*)(id, SEL, struct cg_rect))objc_msgSend)(
        repo_button, sel_registerName("setFrame:"),
        (struct cg_rect){{16, 218}, {width - 166, 34}});
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        repo_button, sel_registerName("setTitle:forState:"),
        string_from_utf8(tr7("Repositorio", "Repository", "Dépôt", "Repository",
            "代码仓库", "程式碼庫", "リポジトリ")), 0);
    ((void (*)(id, SEL, id))objc_msgSend)(
        repo_button, sel_registerName("setBackgroundColor:"),
        ((id (*)(id, SEL, double, double))objc_msgSend)(
            color_class, sel_registerName("colorWithWhite:alpha:"), 0.16, 1.0));
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        repo_button, sel_registerName("setTitleColor:forState:"), red, 0);
    id repo_layer = ((id (*)(id, SEL))objc_msgSend)(
        repo_button, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        repo_layer, sel_registerName("setCornerRadius:"), 10.0);
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(
        repo_button, sel_registerName("addTarget:action:forControlEvents:"),
        info_target, sel_registerName("openRepository:"), 1UL << 6);
    ((void (*)(id, SEL, id))objc_msgSend)(
        scroll, sel_registerName("addSubview:"), repo_button);

    id coffee_button = ((id (*)(id, SEL, long))objc_msgSend)(
        button_class, sel_registerName("buttonWithType:"), 1);
    ((void (*)(id, SEL, struct cg_rect))objc_msgSend)(
        coffee_button, sel_registerName("setFrame:"),
        (struct cg_rect){{16, 260}, {width - 32, 36}});
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        coffee_button, sel_registerName("setTitle:forState:"),
        string_from_utf8("☕ Buy Me a Coffee"), 0);
    ((void (*)(id, SEL, id))objc_msgSend)(
        coffee_button, sel_registerName("setBackgroundColor:"),
        ((id (*)(id, SEL, double, double))objc_msgSend)(
            color_class, sel_registerName("colorWithWhite:alpha:"), 0.16, 1.0));
    ((void (*)(id, SEL, id, NSUInteger))objc_msgSend)(
        coffee_button, sel_registerName("setTitleColor:forState:"), white, 0);
    id coffee_layer = ((id (*)(id, SEL))objc_msgSend)(
        coffee_button, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        coffee_layer, sel_registerName("setCornerRadius:"), 10.0);
    ((void (*)(id, SEL, id, SEL, NSUInteger))objc_msgSend)(
        coffee_button, sel_registerName("addTarget:action:forControlEvents:"),
        info_target, sel_registerName("openBuyMeACoffee:"), 1UL << 6);
    ((void (*)(id, SEL, id))objc_msgSend)(
        scroll, sel_registerName("addSubview:"), coffee_button);

    char ssid[128] = {0}, bssid[64] = {0};
    char ipv4[32] = {0}, mask[32] = {0}, ipv6[INET6_ADDRSTRLEN] = {0};
    char gateway_ip[32] = {0}, gateway_mac[32] = {0}, phone_mac[32] = {0};
    char dns[192] = {0};
    get_current_wifi_identity(ssid, sizeof(ssid), bssid, sizeof(bssid));
    get_interface_ip("en0", ipv4, sizeof(ipv4));
    get_interface_netmask("en0", mask, sizeof(mask));
    get_interface_ipv6("en0", ipv6, sizeof(ipv6));
    get_interface_mac("en0", phone_mac, sizeof(phone_mac));
    get_gateway(gateway_ip, sizeof(gateway_ip), gateway_mac, sizeof(gateway_mac));
    get_dns_servers(dns, sizeof(dns));

    struct cg_rect card_frame = {{16, 312}, {width - 32, 394}};
    id card = ((id (*)(id, SEL))objc_msgSend)(
        view_class, sel_registerName("alloc"));
    card = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(
        card, sel_registerName("initWithFrame:"), card_frame);
    id card_color = ((id (*)(id, SEL, double, double))objc_msgSend)(
        color_class, sel_registerName("colorWithWhite:alpha:"), 0.11, 0.98);
    ((void (*)(id, SEL, id))objc_msgSend)(
        card, sel_registerName("setBackgroundColor:"), card_color);
    id card_layer = ((id (*)(id, SEL))objc_msgSend)(
        card, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        card_layer, sel_registerName("setCornerRadius:"), 18.0);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        card_layer, sel_registerName("setMasksToBounds:"), 1);
    ((void (*)(id, SEL, id))objc_msgSend)(
        scroll, sel_registerName("addSubview:"), card);

    add_info_label(card, (struct cg_rect){{18, 15}, {card_frame.size.width - 36, 28}},
        tr7("RED WI-FI ACTUAL", "CURRENT WI-FI", "WI-FI ACTUEL",
            "AKTUELLES WI-FI", "当前 WI-FI", "目前 WI-FI", "現在の WI-FI"),
        15.0, 0.62, red, 0L, 1L);
    char network_text[1024];
    snprintf(network_text, sizeof(network_text),
        "SSID\n%s\n\nBSSID\n%s\n\n%s\n%s · %s\n\n%s\n%s · %s\n\n%s\n%s\n\nIPv6\n%s\n\nDNS\n%s",
        ssid[0] ? ssid : tr7("No disponible", "Unavailable", "Indisponible",
            "Nicht verfügbar", "不可用", "無法取得", "利用不可"),
        bssid[0] ? bssid : tr7("No disponible", "Unavailable", "Indisponible",
            "Nicht verfügbar", "不可用", "無法取得", "利用不可"),
        tr7("IPv4 · Máscara", "IPv4 · Subnet mask", "IPv4 · Masque",
            "IPv4 · Netzmaske", "IPv4 · 子网掩码", "IPv4 · 子網路遮罩", "IPv4 · サブネットマスク"),
        ipv4[0] ? ipv4 : "-", mask[0] ? mask : "-",
        tr7("Router · MAC", "Router · MAC", "Routeur · MAC", "Router · MAC",
            "路由器 · MAC", "路由器 · MAC", "ルーター · MAC"),
        gateway_ip[0] ? gateway_ip : "-", gateway_mac[0] ? gateway_mac : "-",
        tr7("MAC del iPhone", "iPhone MAC", "MAC de l’iPhone",
            "iPhone-MAC", "iPhone MAC", "iPhone MAC", "iPhone の MAC"),
        phone_mac[0] ? phone_mac : "-",
        ipv6[0] ? ipv6 : "-",
        dns[0] ? dns : "-");
    add_info_label(card,
        (struct cg_rect){{18, 50}, {card_frame.size.width - 36, 332}},
        network_text, 12.5, 0.22, white, 0L, 0L);
    ((void (*)(id, SEL))objc_msgSend)(card, sel_registerName("release"));

    struct cg_rect explain_frame = {{16, 724}, {width - 32, 436}};
    id explain = ((id (*)(id, SEL))objc_msgSend)(
        view_class, sel_registerName("alloc"));
    explain = ((id (*)(id, SEL, struct cg_rect))objc_msgSend)(
        explain, sel_registerName("initWithFrame:"), explain_frame);
    ((void (*)(id, SEL, id))objc_msgSend)(
        explain, sel_registerName("setBackgroundColor:"),
        ((id (*)(id, SEL, double, double))objc_msgSend)(
            color_class, sel_registerName("colorWithWhite:alpha:"), 0.10, 0.98));
    id explain_layer = ((id (*)(id, SEL))objc_msgSend)(
        explain, sel_registerName("layer"));
    ((void (*)(id, SEL, double))objc_msgSend)(
        explain_layer, sel_registerName("setCornerRadius:"), 18.0);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(
        explain_layer, sel_registerName("setMasksToBounds:"), 1);
    ((void (*)(id, SEL, id))objc_msgSend)(
        scroll, sel_registerName("addSubview:"), explain);

    add_info_label(explain,
        (struct cg_rect){{18, 15}, {explain_frame.size.width - 36, 28}},
        tr7("CÓMO FUNCIONA", "HOW IT WORKS", "FONCTIONNEMENT",
            "SO FUNKTIONIERT ES", "工作原理", "運作方式", "仕組み"),
        15.0, 0.62, red, 0L, 1L);
    const char *explanation = tr7(
        "Nuke Wireless usa ARP spoofing en redes IPv4. Cuando bloqueas un equipo, el iPhone envía respuestas ARP manipuladas para hacer que ese equipo asocie la IP del router con una dirección MAC incorrecta y, al mismo tiempo, mantiene el proceso activo para sostener esa asociación. El equipo deja de poder comunicarse correctamente con la puerta de enlace y pierde acceso a Internet mientras el bloqueo está activo.\n\nAl desbloquear, Nuke Wireless detiene el proceso y envía información ARP correcta para ayudar a restaurar la asociación real entre el equipo y el router.\n\nEsto actúa dentro de la red local. No desconecta al cliente del punto de acceso Wi-Fi y no equivale a una desautenticación 802.11. El mecanismo de bloqueo ARP de esta versión afecta a IPv4; el tráfico IPv6 puede seguir funcionando si la red y el dispositivo lo utilizan.",
        "Nuke Wireless uses ARP spoofing on IPv4 networks. When you block a device, the iPhone sends crafted ARP replies so that the target associates the router's IP address with an incorrect MAC address, while keeping the process active to maintain that association. The target can no longer communicate correctly with the gateway and loses Internet access while the block is active.\n\nWhen you unblock it, Nuke Wireless stops the process and sends correct ARP information to help restore the real association between the device and the router.\n\nThis works inside the local network. It does not disconnect the client from the Wi-Fi access point and is not 802.11 deauthentication. This version's ARP blocking affects IPv4; IPv6 traffic may continue to work when the network and device use it.",
        "Nuke Wireless utilise l’ARP spoofing sur les réseaux IPv4. Lorsqu’un appareil est bloqué, l’iPhone envoie des réponses ARP modifiées afin que la cible associe l’adresse IP du routeur à une mauvaise adresse MAC, puis maintient ce processus actif. La cible ne communique plus correctement avec la passerelle et perd l’accès à Internet tant que le blocage reste actif.\n\nLors du déblocage, Nuke Wireless arrête le processus et envoie des informations ARP correctes afin d’aider à rétablir l’association réelle entre l’appareil et le routeur.\n\nCette technique agit sur le réseau local. Elle ne déconnecte pas le client du point d’accès Wi-Fi et ne constitue pas une désauthentification 802.11. Le blocage ARP de cette version concerne IPv4 ; le trafic IPv6 peut continuer à fonctionner.",
        "Nuke Wireless verwendet ARP-Spoofing in IPv4-Netzen. Beim Blockieren sendet das iPhone manipulierte ARP-Antworten, sodass das Zielgerät die IP-Adresse des Routers mit einer falschen MAC-Adresse verknüpft. Der Vorgang bleibt aktiv, um diese Zuordnung aufrechtzuerhalten. Dadurch kann das Ziel nicht mehr korrekt mit dem Gateway kommunizieren und verliert während der Blockierung den Internetzugang.\n\nBeim Entsperren stoppt Nuke Wireless den Vorgang und sendet korrekte ARP-Informationen, um die echte Zuordnung zwischen Gerät und Router wiederherzustellen.\n\nDies wirkt nur im lokalen Netz. Der Client wird nicht vom WLAN-Zugangspunkt getrennt und es handelt sich nicht um eine 802.11-Deauthentifizierung. Die ARP-Sperre dieser Version betrifft IPv4; IPv6-Verkehr kann weiterhin funktionieren.",
        "Nuke Wireless 在 IPv4 网络中使用 ARP 欺骗。阻止设备时，iPhone 会发送经过构造的 ARP 响应，使目标设备把路由器的 IP 地址关联到错误的 MAC 地址，并持续维持这种关联。目标设备因此无法正常与网关通信，在阻止期间会失去互联网访问。\n\n解除阻止时，Nuke Wireless 会停止该过程，并发送正确的 ARP 信息，帮助恢复设备与路由器之间的真实关联。\n\n该机制只作用于局域网，不会让客户端从 Wi-Fi 接入点断开，也不是 802.11 去认证。本版本的 ARP 阻止针对 IPv4；如果网络和设备使用 IPv6，IPv6 流量仍可能继续工作。",
        "Nuke Wireless 在 IPv4 網路中使用 ARP 欺騙。封鎖裝置時，iPhone 會傳送經過設計的 ARP 回覆，使目標裝置把路由器 IP 位址對應到錯誤的 MAC 位址，並持續維持此對應。目標裝置因此無法正常與閘道器通訊，在封鎖期間會失去網際網路連線。\n\n解除封鎖時，Nuke Wireless 會停止該程序並傳送正確的 ARP 資訊，協助恢復裝置與路由器之間的真正對應。\n\n此機制只作用於區域網路，不會讓用戶端從 Wi-Fi 存取點斷線，也不是 802.11 取消驗證。本版本的 ARP 封鎖針對 IPv4；若網路與裝置使用 IPv6，IPv6 流量仍可能繼續運作。",
        "Nuke Wireless は IPv4 ネットワークで ARP スプーフィングを使用します。端末をブロックすると、iPhone は細工した ARP 応答を送信し、対象端末がルーターの IP アドレスを誤った MAC アドレスに対応付けるようにします。その対応を維持するため処理は継続され、対象端末はゲートウェイと正常に通信できなくなり、ブロック中はインターネットへ接続できなくなります。\n\nブロック解除時は処理を停止し、正しい ARP 情報を送信して端末とルーターの本来の対応関係の復旧を助けます。\n\nこれはローカルネットワーク内で作用します。Wi-Fi アクセスポイントから端末を切断するものではなく、802.11 の deauthentication でもありません。このバージョンの ARP ブロックは IPv4 が対象で、IPv6 通信は継続する場合があります。");
    add_info_label(explain,
        (struct cg_rect){{18, 50}, {explain_frame.size.width - 36, 370}},
        explanation, 12.5, 0.20, white, 0L, 0L);
    ((void (*)(id, SEL))objc_msgSend)(explain, sel_registerName("release"));

    add_info_label(scroll, (struct cg_rect){{16, 1176}, {width - 32, 54}},
        tr7("Úsalo únicamente en redes y dispositivos que administras.",
            "Use it only on networks and devices you administer.",
            "Utilisez-le uniquement sur les réseaux et appareils que vous administrez.",
            "Nur in Netzwerken und auf Geräten verwenden, die du verwaltest.",
            "仅在你管理的网络和设备上使用。",
            "僅在你管理的網路和裝置上使用。",
            "自分が管理するネットワークと端末でのみ使用してください。"),
        12.0, 0.20, secondary, 1L, 2L);

    ((void (*)(id, SEL))objc_msgSend)(scroll, sel_registerName("release"));
    debug_line("info-screen", "rendered");
}

static void patched_did_appear(id self, SEL cmd, BOOL animated) {
    id tab = ((id (*)(id, SEL))objc_msgSend)(self,
        sel_registerName("tabBarController"));
    if (!tab && contains(class_getName(object_getClass(self)), "TabBarController"))
        tab = self;
    if (tab) {
        localize_tab_items(tab);
        NSUInteger before_selected = ((NSUInteger (*)(id, SEL))objc_msgSend)(
            tab, sel_registerName("selectedIndex"));
        id before_selected_controller = ((id (*)(id, SEL))objc_msgSend)(
            tab, sel_registerName("selectedViewController"));
        id before_content_controller =
            content_controller_for(before_selected_controller);
        localize_controller_title(before_content_controller, before_selected);
    }
    original_did_appear(self, cmd, animated);
    if (!tab) {
        id view = ((id (*)(id, SEL))objc_msgSend)(
            self, sel_registerName("view"));
        rebrand_visible_view(view, 0);
        return;
    }
    NSUInteger selected = ((NSUInteger (*)(id, SEL))objc_msgSend)(tab,
        sel_registerName("selectedIndex"));
    localize_tab_items(tab);
    id tab_view = ((id (*)(id, SEL))objc_msgSend)(tab, sel_registerName("view"));
    id selected_controller = ((id (*)(id, SEL))objc_msgSend)(tab,
        sel_registerName("selectedViewController"));
    id content_controller = content_controller_for(selected_controller);
    localize_controller_title(content_controller, selected);
    rebrand_controller(content_controller);
    rebrand_visible_view(tab_view, 0);
    if (selected == 0) {
        presenting_controller = content_controller;
        attach_bulk_button(tab_view);
        id wifi_view = ((id (*)(id, SEL))objc_msgSend)(
            presenting_controller, sel_registerName("view"));
        attach_brand_header(wifi_view);
        hide_underlying_brand_text(wifi_view, 0);
        attach_refresh_control(wifi_view);
        if (bulk_panel) ((void (*)(id, SEL, BOOL))objc_msgSend)(bulk_panel,
            sel_registerName("setHidden:"), 0);
        update_dashboard();
        return;
    }
    if (bulk_panel) ((void (*)(id, SEL, BOOL))objc_msgSend)(bulk_panel,
        sel_registerName("setHidden:"), 1);
    if (selected != 2) return;
    info_presenting_controller = content_controller;
    id view = ((id (*)(id, SEL))objc_msgSend)(
        content_controller, sel_registerName("view"));
    render_info_screen(view);
}

static void patched_arguments(id self, SEL cmd, id arguments) {
    if (!arguments) {
        original_arguments(self, cmd, arguments);
        return;
    }
    NSUInteger count = ((NSUInteger (*)(id, SEL))objc_msgSend)(
        arguments, sel_registerName("count"));
    int is_arpoison = 0;
    for (NSUInteger i = 0; i < count; ++i) {
        id item = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            arguments, sel_registerName("objectAtIndex:"), i);
        if (contains(utf8(item), "arpoison")) { is_arpoison = 1; break; }
    }
    char gateway_ip[32], gateway_mac[32];
    int has_gateway = is_arpoison && get_gateway(gateway_ip, sizeof(gateway_ip),
                                                gateway_mac, sizeof(gateway_mac));
    if (is_arpoison) debug_line("gateway-found", has_gateway ? "yes" : "no");
    char phone_mac[32] = {0};
    int has_phone_mac = is_arpoison && get_interface_mac("en0", phone_mac, sizeof(phone_mac));
    if (is_arpoison) debug_line("phone-mac", has_phone_mac ? phone_mac : "missing");
    if (is_arpoison && !repair_in_progress)
        memset(&pending_block, 0, sizeof(pending_block));
    Class array_class = objc_getClass("NSMutableArray");
    id result = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
        array_class, sel_registerName("arrayWithCapacity:"), count);
    for (NSUInteger i = 0; i < count; ++i) {
        id item = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
            arguments, sel_registerName("objectAtIndex:"), i);
        if (is_arpoison && i > 0) {
            id previous = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(
                arguments, sel_registerName("objectAtIndex:"), i - 1);
            const char *flag = utf8(previous);
            if (equals(flag, "-s") && utf8(item))
                snprintf(pending_block.ip, sizeof(pending_block.ip), "%s", utf8(item));
            if (equals(flag, "-r") && utf8(item))
                snprintf(pending_block.real_mac, sizeof(pending_block.real_mac), "%s", utf8(item));
            if (has_gateway && (!utf8(item) || !utf8(item)[0])) {
                if (equals(flag, "-d")) item = string_from_utf8(gateway_ip);
                else if (equals(flag, "-t")) item = string_from_utf8(gateway_mac);
            }
            if (equals(flag, "-r") && has_phone_mac && !repair_in_progress)
                item = string_from_utf8(phone_mac);
        }
        ((void (*)(id, SEL, id))objc_msgSend)(
            result, sel_registerName("addObject:"), rewrite_argument(item));
    }
    original_arguments(self, cmd, result);
    if (capture_root_task) {
        id task_path = ((id (*)(id, SEL))objc_msgSend)(self,
            sel_registerName("launchPath"));
        if (contains(utf8(task_path), "/harpy-reloaded/aegis"))
            captured_root_task = self;
    }
    if (!is_arpoison) {
        id task_path = ((id (*)(id, SEL))objc_msgSend)(self,
            sel_registerName("launchPath"));
        debug_line("task-args-path", utf8(task_path));
        for (NSUInteger j = 0; j < count; ++j) {
            id arg = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(result,
                sel_registerName("objectAtIndex:"), j);
            debug_line("task-arg", utf8(arg));
        }
    }
    if (is_arpoison && !repair_in_progress && pending_block.ip[0]) {
        char arp_mac[32];
        if (get_mac_from_arp_table(pending_block.ip, arp_mac, sizeof(arp_mac)))
            snprintf(pending_block.real_mac, sizeof(pending_block.real_mac), "%s", arp_mac);
        debug_line("block-target", pending_block.ip);
        debug_line("block-real-mac", pending_block.real_mac);
    }
}

__attribute__((constructor)) static void install_paths(void) {
    Method exists = class_getInstanceMethod(objc_getClass("NSFileManager"),
        sel_registerName("fileExistsAtPath:"));
    Class task_class = objc_getClass("NSConcreteTask");
    if (!task_class) task_class = objc_getClass("NSTask");
    Method launch_path = class_getInstanceMethod(task_class,
        sel_registerName("setLaunchPath:"));
    Method arguments = class_getInstanceMethod(task_class,
        sel_registerName("setArguments:"));
    Method terminate = class_getInstanceMethod(task_class,
        sel_registerName("terminate"));
    Method interrupt = class_getInstanceMethod(task_class,
        sel_registerName("interrupt"));
    Method launch = class_getInstanceMethod(task_class,
        sel_registerName("launch"));
    Class commands = objc_getClass("_TtC13HarpyReloaded10MCCommands");
    Method running_ip = commands ? class_getClassMethod(commands,
        sel_registerName("runningBlocksForIpWithIp:")) : 0;
    Method running_arp = commands ? class_getClassMethod(commands,
        sel_registerName("runningBlocksForArp")) : 0;
    Method did_appear = class_getInstanceMethod(objc_getClass("UIViewController"),
        sel_registerName("viewDidAppear:"));
    Method tab_item_set_title = class_getInstanceMethod(
        objc_getClass("UITabBarItem"), sel_registerName("setTitle:"));
    Method navigation_item_set_title = class_getInstanceMethod(
        objc_getClass("UINavigationItem"), sel_registerName("setTitle:"));
    Method controller_set_title = class_getInstanceMethod(
        objc_getClass("UIViewController"), sel_registerName("setTitle:"));
    Class scanner = objc_getClass("_TtC13HarpyReloaded10LanScanner");
    Method scanner_init = scanner ? class_getInstanceMethod(scanner,
        sel_registerName("initWithDelegate:andEnableHotspot:")) : 0;
    Method scanner_start = scanner ? class_getInstanceMethod(scanner,
        sel_registerName("start")) : 0;
    Method scanner_finished = scanner ? class_getInstanceMethod(scanner,
        sel_registerName("lanScanDidFinishScanningWithStatus:")) : 0;
    Method scanner_failed = scanner ? class_getInstanceMethod(scanner,
        sel_registerName("lanScanDidFailedToScan")) : 0;
    Method found_device = scanner ? class_getInstanceMethod(scanner,
        sel_registerName("lanScanDidFindNewDevice:")) : 0;
    if (exists) original_exists = (void *)method_setImplementation(exists, (IMP)patched_exists);
    if (launch_path) original_launch_path = (void *)method_setImplementation(launch_path, (IMP)patched_launch_path);
    if (arguments) original_arguments = (void *)method_setImplementation(arguments, (IMP)patched_arguments);
    if (terminate) original_terminate = (void *)method_setImplementation(terminate, (IMP)patched_terminate);
    if (interrupt) original_interrupt = (void *)method_setImplementation(interrupt, (IMP)patched_interrupt);
    if (launch) original_launch = (void *)method_setImplementation(launch, (IMP)patched_launch);
    if (running_ip) method_setImplementation(running_ip, (IMP)patched_running_ip);
    if (running_arp) method_setImplementation(running_arp, (IMP)patched_running_arp);
    if (tab_item_set_title)
        original_tab_item_set_title = (void *)method_setImplementation(
            tab_item_set_title, (IMP)patched_tab_item_set_title);
    if (navigation_item_set_title)
        original_navigation_item_set_title = (void *)method_setImplementation(
            navigation_item_set_title, (IMP)patched_navigation_item_set_title);
    if (controller_set_title)
        original_controller_set_title = (void *)method_setImplementation(
            controller_set_title, (IMP)patched_controller_set_title);
    if (did_appear) original_did_appear = (void *)method_setImplementation(did_appear, (IMP)patched_did_appear);
    if (scanner_init) original_scanner_init = (void *)method_setImplementation(
        scanner_init, (IMP)patched_scanner_init);
    if (scanner_start) original_scanner_start = (void *)method_setImplementation(
        scanner_start, (IMP)patched_scanner_start);
    if (scanner_finished) original_scanner_finished = (void *)method_setImplementation(
        scanner_finished, (IMP)patched_scanner_finished);
    if (scanner_failed) original_scanner_failed = (void *)method_setImplementation(
        scanner_failed, (IMP)patched_scanner_failed);
    if (found_device) {
        original_found_device = (void *)method_setImplementation(found_device,
            (IMP)patched_found_device);
        debug_line("scan-hook", "installed");
    } else debug_line("scan-hook", "unavailable");
    void (*hook_function)(void *, void *, void **) = (void *)dlsym((void *)-2,
        "MSHookFunction");
    const char *(*image_header)(unsigned) = (void *)dlsym((void *)-2,
        "_dyld_get_image_header");
    const char *(*image_name)(unsigned) = (void *)dlsym((void *)-2,
        "_dyld_get_image_name");
    unsigned (*image_count)(void) = (void *)dlsym((void *)-2,
        "_dyld_image_count");
    const char *app_header = 0;
    if (image_header && image_name && image_count) {
        for (unsigned i = 0; i < image_count(); ++i) {
            const char *name = image_name(i);
            if (contains(name, "/HarpyReloaded.app/HarpyReloaded")) {
                app_header = image_header(i);
                break;
            }
        }
    }
    if (hook_function && app_header) {
        swift_string_to_nsstring = (void *)(app_header + 0x1152fc);
        void *target = (void *)(app_header + 0x39610);
        hook_function(target, (void *)patched_swift_unblock,
            (void **)&original_swift_unblock);
        debug_line("swift-unblock-hook", "installed");
    } else {
        debug_line("swift-unblock-hook", "unavailable");
    }
}


