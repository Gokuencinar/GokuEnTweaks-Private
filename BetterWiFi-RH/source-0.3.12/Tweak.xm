#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <limits.h>
#import <netdb.h>
#import <dlfcn.h>

static NSString * const BWRHPrefsDomain = @"com.betterwifirh.preferences";
static const void *BWRHRefreshControlKey = &BWRHRefreshControlKey;
static const void *BWRHLastCellDetailsKey = &BWRHLastCellDetailsKey;
static const void *BWRHCurrentSubtitleDetailsKey = &BWRHCurrentSubtitleDetailsKey;
static const void *BWRHDetailsButtonKey = &BWRHDetailsButtonKey;
static NSArray *BWRHLastNetworks = nil;
static NSString * const BWRHPrefsChangedNotification = @"com.betterwifirh.preferences.changed";
static __weak UIViewController *BWRHActiveAirportController = nil;

#pragma mark - Preferences / helpers

static NSUserDefaults *BWRHDefaults(void) {
    static NSUserDefaults *defaults;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        defaults = [[NSUserDefaults alloc] initWithSuiteName:BWRHPrefsDomain];
    });
    return defaults;
}

static BOOL BWRHBool(NSString *key, BOOL fallback) {
    id value = [BWRHDefaults() objectForKey:key];
    return value ? [value boolValue] : fallback;
}

static NSInteger BWRHInteger(NSString *key, NSInteger fallback) {
    id value = [BWRHDefaults() objectForKey:key];
    return value ? [value integerValue] : fallback;
}

static BOOL BWRHResponds(id obj, NSString *name) {
    return obj && [obj respondsToSelector:NSSelectorFromString(name)];
}

static BOOL BWRHMsgBool(id obj, NSString *name, BOOL fallback) {
    SEL sel = NSSelectorFromString(name);
    if (!obj || ![obj respondsToSelector:sel]) return fallback;
    return ((BOOL (*)(id, SEL))objc_msgSend)(obj, sel);
}

static long long BWRHMsgLongLong(id obj, NSString *name, long long fallback) {
    SEL sel = NSSelectorFromString(name);
    if (!obj || ![obj respondsToSelector:sel]) return fallback;
    return ((long long (*)(id, SEL))objc_msgSend)(obj, sel);
}

static unsigned long long BWRHMsgUnsignedLongLong(id obj, NSString *name, unsigned long long fallback) {
    SEL sel = NSSelectorFromString(name);
    if (!obj || ![obj respondsToSelector:sel]) return fallback;
    return ((unsigned long long (*)(id, SEL))objc_msgSend)(obj, sel);
}

static int BWRHMsgInt(id obj, NSString *name, int fallback) {
    SEL sel = NSSelectorFromString(name);
    if (!obj || ![obj respondsToSelector:sel]) return fallback;
    return ((int (*)(id, SEL))objc_msgSend)(obj, sel);
}

static id BWRHMsgObject(id obj, NSString *name) {
    SEL sel = NSSelectorFromString(name);
    if (!obj || ![obj respondsToSelector:sel]) return nil;
    return ((id (*)(id, SEL))objc_msgSend)(obj, sel);
}

static NSString *BWRHStringValue(id value) {
    if ([value isKindOfClass:[NSString class]]) return value;
    if ([value respondsToSelector:@selector(stringValue)]) return [value stringValue];
    return nil;
}

static __attribute__((unused)) BOOL BWRHSpanish(void) {
    static BOOL spanish = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *language = [NSLocale preferredLanguages].firstObject ?: @"en";
        spanish = [language.lowercaseString hasPrefix:@"es"];
    });
    return spanish;
}

static NSInteger BWRHLanguageMode(void) {
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:BWRHPrefsDomain];
    id value = [defaults objectForKey:@"language"];
    NSInteger mode = value ? [value integerValue] : 0;
    return (mode >= 0 && mode <= 2) ? mode : 0;
}

static BOOL BWRHSystemUsesSpanish(void) {
    NSString *language = [NSLocale preferredLanguages].firstObject ?: @"en";
    return [[language lowercaseString] hasPrefix:@"es"];
}

static NSString *BWRHT(NSString *es, NSString *en) {
    NSInteger mode = BWRHLanguageMode();
    if (mode == 1) return es;
    if (mode == 2) return en;
    return BWRHSystemUsesSpanish() ? es : en;
}

#pragma mark - Wi-Fi data

static NSString *BWRHNetworkSSID(id network) {
    id rawSSID = BWRHMsgObject(network, @"ssid");
    NSString *ssid = BWRHStringValue(rawSSID);
    if (!ssid.length && [rawSSID isKindOfClass:[NSData class]]) {
        ssid = [[NSString alloc] initWithData:rawSSID encoding:NSUTF8StringEncoding];
    }
    if (ssid.length) return ssid;
    NSString *networkName = BWRHStringValue(BWRHMsgObject(network, @"networkName"));
    if (networkName.length) return networkName;
    NSString *title = BWRHStringValue(BWRHMsgObject(network, @"title"));
    return title ?: @"";
}

static NSString *BWRHSecurityDescription(id network) {
    if (!network) return @"";

    id scanResult = BWRHMsgObject(network, @"scanResult");
    id source = scanResult ?: network;

    if (BWRHMsgBool(source, @"isOpen", NO) || !BWRHMsgBool(network, @"isSecure", YES)) {
        return BWRHT(@"Abierta", @"Open");
    }

    NSMutableArray<NSString *> *parts = [NSMutableArray array];
    if (BWRHMsgBool(source, @"isOWE", NO)) [parts addObject:@"OWE"];
    if (BWRHMsgBool(source, @"isWEP", NO)) [parts addObject:@"WEP"];
    if (BWRHMsgBool(source, @"isWPA", NO)) [parts addObject:@"WPA"];
    if (BWRHMsgBool(source, @"isWPA2", NO)) [parts addObject:@"WPA2"];
    if (BWRHMsgBool(source, @"isWPA3", NO)) [parts addObject:@"WPA3"];
    if (BWRHMsgBool(source, @"isEAP", NO) || BWRHMsgBool(network, @"isEnterprise", NO)) {
        [parts addObject:@"Enterprise"];
    }

    if (parts.count == 0) {
        long long mode = BWRHMsgLongLong(network, @"securityMode", -1);
        if (mode == 0) return BWRHT(@"Abierta", @"Open");
        return BWRHT(@"Protegida", @"Secured");
    }
    return [parts componentsJoinedByString:@"/"];
}

static id BWRHCWFChannel(id network) {
    id scanResult = BWRHMsgObject(network, @"scanResult");
    return BWRHMsgObject(scanResult, @"channel");
}

static NSNumber *BWRHChannelNumber(id network) {
    id channel = BWRHMsgObject(network, @"channel");
    if ([channel isKindOfClass:[NSNumber class]]) return channel;

    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"channel")) {
        unsigned long long value = BWRHMsgUnsignedLongLong(cwfChannel, @"channel", 0);
        if (value > 0 && value < 1000) return @(value);
    }
    return nil;
}

static NSNumber *BWRHChannelWidth(id network) {
    id width = BWRHMsgObject(network, @"channelWidth");
    if ([width isKindOfClass:[NSNumber class]]) {
        NSInteger raw = [width integerValue];
        if (raw == 20 || raw == 40 || raw == 80 || raw == 160 || raw == 320) return width;
        if (raw >= 1 && raw <= 5) {
            static NSInteger widths[] = {0, 20, 40, 80, 160, 160};
            return @(widths[raw]);
        }
    }

    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"width")) {
        int raw = BWRHMsgInt(cwfChannel, @"width", -1);
        switch (raw) {
            case 1: return @20;
            case 2: return @40;
            case 3: return @80;
            case 4: return @160;
            case 5: return @160;
            default: break;
        }
    }
    return nil;
}

static NSString *BWRHBandDescription(id network) {
    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel) {
        if (BWRHMsgBool(cwfChannel, @"is2GHz", NO)) return @"2.4 GHz";
        if (BWRHMsgBool(cwfChannel, @"is5GHz", NO)) return @"5 GHz";
        if (BWRHMsgBool(cwfChannel, @"is6GHz", NO)) return @"6 GHz";
    }

    NSNumber *ch = BWRHChannelNumber(network);
    if (ch) {
        NSInteger value = ch.integerValue;
        if (value >= 1 && value <= 14) return @"2.4 GHz";
        if (value >= 32 && value <= 177) return @"5 GHz";
        if (value >= 178 && value <= 233) return @"6 GHz";
    }
    return @"";
}

static NSString *BWRHBSSID(id network) {
    id value = BWRHMsgObject(network, @"bssid");
    if ([value isKindOfClass:[NSString class]] && [value length]) return value;
    value = BWRHMsgObject(network, @"BSSID");
    if ([value isKindOfClass:[NSString class]] && [value length]) return value;

    id scanResult = BWRHMsgObject(network, @"scanResult");
    value = BWRHMsgObject(scanResult, @"BSSID");
    if ([value isKindOfClass:[NSString class]] && [value length]) return value;
    value = BWRHMsgObject(scanResult, @"bssid");
    if ([value isKindOfClass:[NSString class]]) return value;
    return @"";
}

static NSString *BWRHSignalQuality(long long rssi) {
    if (rssi >= -50) return BWRHT(@"Excelente", @"Excellent");
    if (rssi >= -60) return BWRHT(@"Muy buena", @"Very good");
    if (rssi >= -67) return BWRHT(@"Buena", @"Good");
    if (rssi >= -75) return BWRHT(@"Aceptable", @"Fair");
    return BWRHT(@"Débil", @"Weak");
}

static NSString *BWRHDetailsForNetwork(id network) {
    NSMutableArray<NSString *> *parts = [NSMutableArray array];

    if (BWRHBool(@"showRSSI", YES)) {
        long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);
        if (rssi != LLONG_MIN && rssi < 0 && rssi > -200) {
            [parts addObject:[NSString stringWithFormat:@"%lld dBm", rssi]];
        }
    }

    if (BWRHBool(@"showSecurity", YES)) {
        NSString *security = BWRHSecurityDescription(network);
        if (security.length) [parts addObject:security];
    }

    if (BWRHBool(@"showChannel", YES)) {
        NSNumber *channel = BWRHChannelNumber(network);
        if (channel) [parts addObject:[NSString stringWithFormat:@"Ch %@", channel]];
    }

    if (BWRHBool(@"showBand", YES)) {
        NSString *band = BWRHBandDescription(network);
        if (band.length) [parts addObject:band];
    }

    if (BWRHBool(@"showBSSID", NO)) {
        NSString *bssid = BWRHBSSID(network);
        if (bssid.length) [parts addObject:bssid];
    }

    return [parts componentsJoinedByString:@" · "];
}

#pragma mark - Network list processing

static long long BWRHNetworkRSSI(id network) {
    if (!network) return LLONG_MIN;
    if (BWRHResponds(network, @"rssi")) {
        long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);
        if (rssi < 0 && rssi > -200) return rssi;
    }
    id scanResult = BWRHMsgObject(network, @"scanResult");
    if (BWRHResponds(scanResult, @"rssi")) {
        long long rssi = BWRHMsgLongLong(scanResult, @"rssi", LLONG_MIN);
        if (rssi < 0 && rssi > -200) return rssi;
    }
    return LLONG_MIN;
}

static BOOL BWRHNetworkIsOpen(id network) {
    if (!network) return NO;
    id scanResult = BWRHMsgObject(network, @"scanResult");
    if (BWRHResponds(scanResult, @"isOpen") && BWRHMsgBool(scanResult, @"isOpen", NO)) return YES;
    if (BWRHResponds(network, @"isSecure")) return !BWRHMsgBool(network, @"isSecure", YES);
    long long mode = BWRHMsgLongLong(network, @"securityMode", -1);
    if (mode == 0) return YES;
    mode = BWRHMsgLongLong(scanResult, @"securityMode", -1);
    if (mode == 0) return YES;
    return NO;
}

static BOOL BWRHNetworkMatchesBand(id network, NSInteger filter) {
    if (filter == 0) return YES;
    NSString *band = BWRHBandDescription(network);
    if (filter == 24) return [band hasPrefix:@"2.4"];
    if (filter == 5) return [band hasPrefix:@"5"];
    if (filter == 6) return [band hasPrefix:@"6"];
    return YES;
}

static BOOL BWRHIsNativeWiFiKitSnapshotObject(id network) {
    if (!network) return NO;
    NSString *className = NSStringFromClass([network class]);
    if ([className isEqualToString:@"BWRHMobileWiFiNetwork"]) return NO;
    return YES;
}

static NSArray *BWRHSnapshotNetworks(void) {
    @synchronized (BWRHPrefsDomain) {
        NSArray *snapshot = [BWRHLastNetworks copy] ?: @[];
        NSMutableArray *safe = [NSMutableArray arrayWithCapacity:snapshot.count];
        for (id network in snapshot) {
            if (BWRHIsNativeWiFiKitSnapshotObject(network)) [safe addObject:network];
        }
        return safe;
    }
}

static void BWRHStoreNetworkSnapshot(NSArray *networks) {
    if (![networks isKindOfClass:[NSArray class]]) return;
    NSMutableArray *safe = [NSMutableArray arrayWithCapacity:networks.count];
    for (id network in networks) {
        if (BWRHIsNativeWiFiKitSnapshotObject(network)) [safe addObject:network];
    }
    @synchronized (BWRHPrefsDomain) {
        BWRHLastNetworks = [safe copy];
    }
}

static id BWRHResolvedCurrentNetwork(id network) {
    if (!network) return nil;

    // Current-network records can contain less scan metadata than the nearby
    // scan record. Prefer the exact/equivalent scan record when available.
    NSArray *snapshot = BWRHSnapshotNetworks();
    if (!snapshot.count) return network;

    NSString *ssid = BWRHNetworkSSID(network);
    unsigned long long uid = BWRHMsgUnsignedLongLong(network, @"uniqueIdentifier", 0);
    id best = nil;
    long long bestRSSI = LLONG_MIN;

    for (id candidate in snapshot) {
        BOOL equivalent = NO;
        SEL eqSel = NSSelectorFromString(@"isEquivalentRecord:");
        if ([network respondsToSelector:eqSel]) {
            equivalent = ((BOOL (*)(id, SEL, id))objc_msgSend)(network, eqSel, candidate);
        }
        if (!equivalent && [candidate respondsToSelector:eqSel]) {
            equivalent = ((BOOL (*)(id, SEL, id))objc_msgSend)(candidate, eqSel, network);
        }

        unsigned long long cuid = BWRHMsgUnsignedLongLong(candidate, @"uniqueIdentifier", 0);
        BOOL uidMatch = uid && cuid && uid == cuid;
        BOOL ssidMatch = ssid.length && [BWRHNetworkSSID(candidate) isEqualToString:ssid];
        if (!(equivalent || uidMatch || ssidMatch)) continue;

        long long rssi = BWRHMsgLongLong(candidate, @"rssi", LLONG_MIN);
        if (!best || equivalent || uidMatch || rssi > bestRSSI) {
            best = candidate;
            bestRSSI = rssi;
            if (equivalent || uidMatch) break;
        }
    }

    return best ?: network;
}

static NSArray *BWRHProcessNetworks(NSArray *networks) {
    if (![networks isKindOfClass:[NSArray class]]) return networks;
    if (!BWRHBool(@"enabled", YES)) return networks;

    NSInteger bandFilter = BWRHInteger(@"bandFilter", 0);
    NSInteger typeFilter = BWRHInteger(@"networkTypeFilter", 0);
    BOOL openOnly = BWRHBool(@"openOnly", NO);
    BOOL thresholdEnabled = BWRHBool(@"minimumRSSIEnabled", NO);
    NSInteger minimumRSSI = BWRHInteger(@"minimumRSSI", -90);

    NSPredicate *predicate = [NSPredicate predicateWithBlock:^BOOL(id network, NSDictionary *bindings) {
        if (openOnly && !BWRHNetworkIsOpen(network)) return NO;
        if (!BWRHNetworkMatchesBand(network, bandFilter)) return NO;

        BOOL known = BWRHMsgBool(network, @"isKnown", NO);
        if (typeFilter == 1 && !known) return NO;
        if (typeFilter == 2 && known) return NO;

        if (thresholdEnabled) {
            long long rssi = BWRHNetworkRSSI(network);
            if (rssi != LLONG_MIN && rssi < minimumRSSI) return NO;
        }
        return YES;
    }];

    NSArray *result = [networks filteredArrayUsingPredicate:predicate];
    NSInteger legacySignalSort = BWRHBool(@"sortBySignal", NO) ? 1 : 0;
    NSInteger sortMode = BWRHInteger(@"sortMode", legacySignalSort);

    if (sortMode != 0) {
        result = [result sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
            if (sortMode == 1) {
                long long arssi = BWRHNetworkRSSI(a);
                long long brssi = BWRHNetworkRSSI(b);
                if (arssi == LLONG_MIN) arssi = -999;
                if (brssi == LLONG_MIN) brssi = -999;
                if (arssi > brssi) return NSOrderedAscending;
                if (arssi < brssi) return NSOrderedDescending;
            } else if (sortMode == 2) {
                NSString *an = BWRHNetworkSSID(a);
                NSString *bn = BWRHNetworkSSID(b);
                NSComparisonResult nameResult = [an localizedCaseInsensitiveCompare:bn];
                if (nameResult != NSOrderedSame) return nameResult;
            } else if (sortMode == 3) {
                NSInteger ac = BWRHChannelNumber(a).integerValue;
                NSInteger bc = BWRHChannelNumber(b).integerValue;
                if (ac > 0 && bc > 0 && ac != bc) return ac < bc ? NSOrderedAscending : NSOrderedDescending;
                if (ac > 0 && bc == 0) return NSOrderedAscending;
                if (ac == 0 && bc > 0) return NSOrderedDescending;
            }

            NSString *atitle = BWRHNetworkSSID(a);
            NSString *btitle = BWRHNetworkSSID(b);
            return [atitle localizedCaseInsensitiveCompare:btitle];
        }];
    }

    return result;
}

static id BWRHProcessNetworkCollection(id networks) {
    if ([networks isKindOfClass:[NSSet class]]) {
        NSArray *processed = BWRHProcessNetworks([(NSSet *)networks allObjects]);
        return [NSSet setWithArray:processed ?: @[]];
    }
    if ([networks isKindOfClass:[NSArray class]]) return BWRHProcessNetworks(networks);
    return networks;
}

static void BWRHStoreNetworkCollection(id networks) {
    if ([networks isKindOfClass:[NSSet class]]) BWRHStoreNetworkSnapshot([(NSSet *)networks allObjects]);
    else if ([networks isKindOfClass:[NSArray class]]) BWRHStoreNetworkSnapshot(networks);
}

#pragma mark - Cell decoration

static void BWRHSetCellSubtitle(id cell, NSString *text) {
    if (!cell || !text.length) return;

    NSString *(^cleanBase)(NSString *) = ^NSString *(NSString *original) {
        NSString *lastDetails = objc_getAssociatedObject(cell, BWRHLastCellDetailsKey);
        if (!original.length || !lastDetails.length) return original ?: @"";
        if ([original isEqualToString:lastDetails]) return @"";
        NSString *suffix = [NSString stringWithFormat:@" · %@", lastDetails];
        if ([original hasSuffix:suffix]) {
            return [original substringToIndex:original.length - suffix.length];
        }
        return original;
    };

    SEL subtitleSel = NSSelectorFromString(@"subtitle");
    SEL setSubtitleSel = NSSelectorFromString(@"setSubtitle:");
    if ([cell respondsToSelector:setSubtitleSel]) {
        NSString *original = nil;
        if ([cell respondsToSelector:subtitleSel]) {
            id value = ((id (*)(id, SEL))objc_msgSend)(cell, subtitleSel);
            if ([value isKindOfClass:[NSString class]]) original = value;
        }
        NSString *base = cleanBase(original);
        NSString *combined = base.length ? [NSString stringWithFormat:@"%@ · %@", base, text] : text;
        ((void (*)(id, SEL, id))objc_msgSend)(cell, setSubtitleSel, combined);
        objc_setAssociatedObject(cell, BWRHLastCellDetailsKey, text, OBJC_ASSOCIATION_COPY_NONATOMIC);
        return;
    }

    if ([cell isKindOfClass:[UITableViewCell class]]) {
        UITableViewCell *tableCell = (UITableViewCell *)cell;
        if (tableCell.detailTextLabel) {
            NSString *base = cleanBase(tableCell.detailTextLabel.text);
            tableCell.detailTextLabel.text = base.length ? [NSString stringWithFormat:@"%@ · %@", base, text] : text;
            objc_setAssociatedObject(cell, BWRHLastCellDetailsKey, text, OBJC_ASSOCIATION_COPY_NONATOMIC);
        }
    }
}

static void BWRHDecorateCell(id cell, id network) {
    if (!BWRHBool(@"enabled", YES) || !cell || !network) return;
    NSString *details = BWRHDetailsForNetwork(network);
    if (details.length) BWRHSetCellSubtitle(cell, details);
}

#pragma mark - Pull to refresh

static void BWRHConfigureRefreshControl(UIViewController *controller) {
    if (!controller || !BWRHBool(@"enabled", YES) || !BWRHBool(@"pullToRefresh", YES)) return;
    if (![controller isKindOfClass:[UITableViewController class]]) return;

    UITableViewController *tableController = (UITableViewController *)controller;
    UIRefreshControl *refresh = objc_getAssociatedObject(controller, BWRHRefreshControlKey);

    if (!refresh) {
        refresh = [[UIRefreshControl alloc] init];
        [refresh addTarget:controller action:NSSelectorFromString(@"bwrh_pullToRefresh:") forControlEvents:UIControlEventValueChanged];
        objc_setAssociatedObject(controller, BWRHRefreshControlKey, refresh, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    tableController.refreshControl = refresh;
}

static void BWRHApplyListPreferences(id controller, BOOL requestScan) {
    if (!controller) return;
    [BWRHDefaults() synchronize];

    if (BWRHBool(@"enabled", YES) && BWRHBool(@"pullToRefresh", YES)) {
        BWRHConfigureRefreshControl((UIViewController *)controller);
    } else if ([controller isKindOfClass:[UITableViewController class]]) {
        ((UITableViewController *)controller).refreshControl = nil;
    }

    NSArray *snapshot = BWRHSnapshotNetworks();
    SEL setNetworksSel = NSSelectorFromString(@"setNetworks:");
    SEL setInfraSel = NSSelectorFromString(@"setInfraNetworks:");
    if (snapshot.count && [controller respondsToSelector:setNetworksSel]) {
        ((void (*)(id, SEL, id))objc_msgSend)(controller, setNetworksSel, [NSSet setWithArray:snapshot]);
    } else if ([controller respondsToSelector:setInfraSel]) {
        id infra = BWRHMsgObject(controller, @"infraNetworks");
        if ([infra isKindOfClass:[NSArray class]]) {
            ((void (*)(id, SEL, id))objc_msgSend)(controller, setInfraSel, infra);
        }
    }

    if ([controller isKindOfClass:[UITableViewController class]]) {
        [((UITableViewController *)controller).tableView reloadData];
    }

    if (requestScan && [controller respondsToSelector:NSSelectorFromString(@"refresh")]) {
        ((void (*)(id, SEL))objc_msgSend)(controller, NSSelectorFromString(@"refresh"));
    }
}

static void BWRHPreferencesChanged(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [BWRHDefaults() synchronize];
        UIViewController *controller = BWRHActiveAirportController;
        if (controller) BWRHApplyListPreferences(controller, YES);
    });
}

#pragma mark - Connected network details

static id BWRHCurrentDetailsNetwork(id controller) {
    id context = BWRHMsgObject(controller, @"context");
    return BWRHMsgObject(context, @"network");
}

static BOOL BWRHShouldShowCurrentDetails(id controller) {
    if (!BWRHBool(@"enabled", YES) || !BWRHBool(@"showConnectedDetails", YES)) return NO;
    id context = BWRHMsgObject(controller, @"context");
    if (!context || !BWRHMsgBool(context, @"isCurrent", NO)) return NO;
    return BWRHCurrentDetailsNetwork(controller) != nil;
}

static NSArray<NSDictionary *> *BWRHCurrentInfoItems(id controller) {
    id network = BWRHResolvedCurrentNetwork(BWRHCurrentDetailsNetwork(controller));
    id config = BWRHMsgObject(controller, @"config");
    id context = BWRHMsgObject(controller, @"context");
    if (!network) return @[];

    NSMutableArray<NSDictionary *> *items = [NSMutableArray array];
    void (^add)(NSString *, NSString *) = ^(NSString *label, NSString *value) {
        if (value.length) [items addObject:@{ @"label": label, @"value": value }];
    };

    if (BWRHBool(@"showRSSI", YES)) {
        long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);
        if (rssi != LLONG_MIN && rssi < 0 && rssi > -200) {
            add(BWRHT(@"Señal", @"Signal"), [NSString stringWithFormat:@"%lld dBm · %@", rssi, BWRHSignalQuality(rssi)]);
        }
    }

    if (BWRHBool(@"showSecurity", YES)) add(BWRHT(@"Seguridad", @"Security"), BWRHSecurityDescription(network));
    if (BWRHBool(@"detailsShowBSSID", YES)) add(@"BSSID", BWRHBSSID(network));

    if (BWRHBool(@"showChannel", YES)) {
        NSNumber *channel = BWRHChannelNumber(network);
        if (channel) add(BWRHT(@"Canal", @"Channel"), channel.stringValue);
    }

    if (BWRHBool(@"showBand", YES)) add(BWRHT(@"Banda", @"Band"), BWRHBandDescription(network));

    if (BWRHBool(@"detailsShowChannelWidth", YES)) {
        NSNumber *width = BWRHChannelWidth(network);
        if (width && width.integerValue > 0) {
            add(BWRHT(@"Ancho de canal", @"Channel width"), [NSString stringWithFormat:@"%@ MHz", width]);
        }
    }

    if (BWRHBool(@"detailsShowIPv4", YES)) add(BWRHT(@"Dirección IPv4", @"IPv4 address"), BWRHStringValue(BWRHMsgObject(config, @"ipv4Address")));
    if (BWRHBool(@"detailsShowRouter", YES)) add(BWRHT(@"Router", @"Router"), BWRHStringValue(BWRHMsgObject(config, @"ipv4RouterAddress")));

    if (BWRHBool(@"detailsShowDNS", YES)) {
        id dns = BWRHMsgObject(config, @"dnsServerAddresses");
        if ([dns isKindOfClass:[NSArray class]] && [dns count]) {
            add(@"DNS", [(NSArray *)dns componentsJoinedByString:@", "]);
        }
    }

    if (BWRHBool(@"detailsShowPrivateAddress", NO)) {
        add(BWRHT(@"Dirección Wi‑Fi privada", @"Private Wi-Fi address"), BWRHStringValue(BWRHMsgObject(context, @"randomMACAddress")));
    }

    if (BWRHBool(@"showTools", YES)) {
        [items addObject:@{ @"label": BWRHT(@"Monitor de señal", @"Signal monitor"), @"value": BWRHT(@"Gráfica en vivo", @"Live graph"), @"action": @"signal" }];
        [items addObject:@{ @"label": BWRHT(@"Analizador de canales", @"Channel analyzer"), @"value": BWRHT(@"Redes cercanas", @"Nearby networks"), @"action": @"channels" }];
        [items addObject:@{ @"label": BWRHT(@"Diagnóstico rápido", @"Quick diagnostics"), @"value": BWRHT(@"DNS e Internet", @"DNS & Internet"), @"action": @"diagnostics" }];
    }

    return items;
}

static UIViewController *BWRHFindAirportController(UIViewController *controller) {
    for (UIViewController *candidate in controller.navigationController.viewControllers.reverseObjectEnumerator) {
        NSString *name = NSStringFromClass(candidate.class);
        if ([name containsString:@"WFAirportViewController"]) return candidate;
    }
    return nil;
}

static id BWRHNetworkListControllerForAirport(id airportController) {
    return BWRHMsgObject(airportController, @"listDelegate");
}

static id BWRHWFInterfaceForAirport(id airportController) {
    return BWRHMsgObject(BWRHNetworkListControllerForAirport(airportController), @"interface");
}

static id BWRHCWFInterfaceForAirport(id airportController) {
    return BWRHMsgObject(BWRHWFInterfaceForAirport(airportController), @"cInterface");
}

static long long BWRHLiveRSSIForAirport(id airportController, id fallbackNetwork) {
    id cInterface = BWRHCWFInterfaceForAirport(airportController);
    long long rssi = BWRHMsgLongLong(cInterface, @"RSSI", LLONG_MIN);
    if (rssi < 0 && rssi > -200) return rssi;
    id listController = BWRHNetworkListControllerForAirport(airportController);
    id linkQuality = BWRHMsgObject(listController, @"latestLinkQuality");
    rssi = BWRHMsgLongLong(linkQuality, @"rssi", LLONG_MIN);
    if (rssi < 0 && rssi > -200) return rssi;
    id current = BWRHMsgObject(BWRHWFInterfaceForAirport(airportController), @"currentNetwork");
    rssi = BWRHMsgLongLong(current, @"rssi", LLONG_MIN);
    if (rssi < 0 && rssi > -200) return rssi;
    rssi = BWRHMsgLongLong(fallbackNetwork, @"rssi", LLONG_MIN);
    return (rssi < 0 && rssi > -200) ? rssi : LLONG_MIN;
}

static id BWRHLiveScanResultForAirport(id airportController) {
    id result = BWRHMsgObject(BWRHCWFInterfaceForAirport(airportController), @"currentScanResult");
    return result ?: BWRHMsgObject(BWRHWFInterfaceForAirport(airportController), @"currentNetwork");
}

static NSArray *BWRHLatestScanNetworks(id airportController) {
    NSMutableArray *out = [NSMutableArray array];
    id listController = BWRHNetworkListControllerForAirport(airportController);
    id candidates = BWRHMsgObject(listController, @"networks");
    if ([candidates isKindOfClass:[NSSet class]]) [out addObjectsFromArray:[candidates allObjects]];
    else if ([candidates isKindOfClass:[NSArray class]]) [out addObjectsFromArray:candidates];
    if (!out.count) {
        id scanManager = BWRHMsgObject(listController, @"scanManager");
        candidates = BWRHMsgObject(scanManager, @"networks");
        if ([candidates isKindOfClass:[NSSet class]]) [out addObjectsFromArray:[candidates allObjects]];
        else if ([candidates isKindOfClass:[NSArray class]]) [out addObjectsFromArray:candidates];
    }
    if (!out.count) {
        candidates = BWRHMsgObject(airportController, @"allNetworks");
        if ([candidates isKindOfClass:[NSSet class]]) [out addObjectsFromArray:[candidates allObjects]];
        else if ([candidates isKindOfClass:[NSArray class]]) [out addObjectsFromArray:candidates];
    }
    if (!out.count) [out addObjectsFromArray:BWRHSnapshotNetworks()];
    return out;
}

static void BWRHRequestFreshScan(id airportController) {
    id listController = BWRHNetworkListControllerForAirport(airportController);
    if (BWRHResponds(listController, @"startScanning")) ((void (*)(id, SEL))objc_msgSend)(listController, NSSelectorFromString(@"startScanning"));
    if (BWRHResponds(airportController, @"refresh")) ((void (*)(id, SEL))objc_msgSend)(airportController, NSSelectorFromString(@"refresh"));
}

#pragma mark - MobileWiFi direct access

typedef void *BWRHMWManagerRef;
typedef void *BWRHMWDeviceRef;
typedef void *BWRHMWNetworkRef;
typedef void (*BWRHMWGenericCallback)(BWRHMWDeviceRef, CFDictionaryRef, const void *);
typedef void (*BWRHMWScanCallback)(BWRHMWDeviceRef, CFArrayRef, int, const void *);

static void *BWRHMWHandle = NULL;
static BWRHMWManagerRef BWRHMWManager = NULL;
static BWRHMWDeviceRef BWRHMWDevice = NULL;
static BOOL BWRHMWLQMRegistered = NO;
static BWRHMWDeviceRef BWRHMWLQMDevice = NULL;
static NSUInteger BWRHMWLQMEventCount = 0;
static NSUInteger BWRHWiFiKitLQEventCount = 0;
static __weak id BWRHMWActiveMonitor = nil;
static __weak id BWRHMWActiveAnalyzer = nil;

static BWRHMWManagerRef (*BWRHMWManagerCreate)(CFAllocatorRef, int) = NULL;
static CFArrayRef (*BWRHMWCopyDevices)(BWRHMWManagerRef) = NULL;
static void (*BWRHMWSchedule)(BWRHMWManagerRef, CFRunLoopRef, CFStringRef) = NULL;
static void (*BWRHMWUnschedule)(BWRHMWManagerRef) = NULL;
static CFTypeRef (*BWRHMWDeviceCopyProperty)(BWRHMWDeviceRef, CFStringRef) = NULL;
static BWRHMWNetworkRef (*BWRHMWCopyCurrentNetwork)(BWRHMWDeviceRef) = NULL;
static CFTypeRef (*BWRHMWNetworkGetProperty)(BWRHMWNetworkRef, CFStringRef) = NULL;
static CFStringRef (*BWRHMWNetworkGetSSID)(BWRHMWNetworkRef) = NULL;
static CFDictionaryRef (*BWRHMWNetworkCopyRecord)(BWRHMWNetworkRef) = NULL;
static int (*BWRHMWNetworkGetRSSI)(BWRHMWNetworkRef) = NULL;
static int (*BWRHMWNetworkGetChannel)(BWRHMWNetworkRef) = NULL;
static int (*BWRHMWScanAsync)(BWRHMWDeviceRef, CFDictionaryRef, BWRHMWScanCallback, const void *) = NULL;
static void (*BWRHMWScanCancel)(BWRHMWDeviceRef) = NULL;
static void (*BWRHMWRegisterLQM)(BWRHMWDeviceRef, BWRHMWGenericCallback, const void *) = NULL;

@interface BWRHMobileWiFiNetwork : NSObject
@property (nonatomic, copy) NSString *ssid;
@property (nonatomic, copy) NSString *bssid;
@property (nonatomic, strong) NSNumber *channel;
@property (nonatomic) long long rssi;
@property (nonatomic) BOOL secure;
@end

@implementation BWRHMobileWiFiNetwork
- (NSString *)title { return self.ssid ?: @""; }
- (BOOL)isSecure { return self.secure; }

// Defensive WiFiKit compatibility only. MobileWiFi analyzer records should never
// be inserted into WFAirportViewController's native network set, but answering
// these KVC-backed flags prevents WiFiKitUI predicates from aborting if one
// accidentally crosses that boundary.
- (BOOL)isKnown { return NO; }
- (BOOL)isInstantHotspot { return NO; }
- (BOOL)isAdhoc { return NO; }
- (BOOL)isUnconfiguredAccessory { return NO; }
- (BOOL)isPopular { return NO; }
@end

static long long BWRHMWRSSIFromValue(CFTypeRef value) {
    if (!value) return LLONG_MIN;
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        long long out = LLONG_MIN;
        if (CFNumberGetValue((CFNumberRef)value, kCFNumberLongLongType, &out)) return out;
        return LLONG_MIN;
    }
    if (CFGetTypeID(value) == CFDictionaryGetTypeID()) {
        CFDictionaryRef dict = (CFDictionaryRef)value;
        const CFStringRef keys[] = {
            CFSTR("RSSI_CTL_AGR"), CFSTR("RSSI"), CFSTR("AVG_RSSI"),
            CFSTR("RSSI_CTL_0"), CFSTR("RSSI_CTL_1")
        };
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            CFTypeRef candidate = CFDictionaryGetValue(dict, keys[i]);
            if (candidate && CFGetTypeID(candidate) == CFNumberGetTypeID()) {
                long long out = LLONG_MIN;
                if (CFNumberGetValue((CFNumberRef)candidate, kCFNumberLongLongType, &out)) return out;
            }
        }
    }
    return LLONG_MIN;
}

static void BWRHMWResolveSymbols(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        BWRHMWHandle = dlopen("/System/Library/PrivateFrameworks/MobileWiFi.framework/MobileWiFi", RTLD_LAZY | RTLD_LOCAL);
        if (!BWRHMWHandle) return;
        BWRHMWManagerCreate = (BWRHMWManagerRef (*)(CFAllocatorRef, int))dlsym(BWRHMWHandle, "WiFiManagerClientCreate");
        BWRHMWCopyDevices = (CFArrayRef (*)(BWRHMWManagerRef))dlsym(BWRHMWHandle, "WiFiManagerClientCopyDevices");
        BWRHMWSchedule = (void (*)(BWRHMWManagerRef, CFRunLoopRef, CFStringRef))dlsym(BWRHMWHandle, "WiFiManagerClientScheduleWithRunLoop");
        BWRHMWUnschedule = (void (*)(BWRHMWManagerRef))dlsym(BWRHMWHandle, "WiFiManagerClientUnscheduleFromRunLoop");
        BWRHMWDeviceCopyProperty = (CFTypeRef (*)(BWRHMWDeviceRef, CFStringRef))dlsym(BWRHMWHandle, "WiFiDeviceClientCopyProperty");
        BWRHMWCopyCurrentNetwork = (BWRHMWNetworkRef (*)(BWRHMWDeviceRef))dlsym(BWRHMWHandle, "WiFiDeviceClientCopyCurrentNetwork");
        BWRHMWNetworkGetProperty = (CFTypeRef (*)(BWRHMWNetworkRef, CFStringRef))dlsym(BWRHMWHandle, "WiFiNetworkGetProperty");
        BWRHMWNetworkGetSSID = (CFStringRef (*)(BWRHMWNetworkRef))dlsym(BWRHMWHandle, "WiFiNetworkGetSSID");
        BWRHMWNetworkCopyRecord = (CFDictionaryRef (*)(BWRHMWNetworkRef))dlsym(BWRHMWHandle, "WiFiNetworkCopyRecord");
        BWRHMWNetworkGetRSSI = (int (*)(BWRHMWNetworkRef))dlsym(BWRHMWHandle, "WiFiNetworkGetRSSI");
        BWRHMWNetworkGetChannel = (int (*)(BWRHMWNetworkRef))dlsym(BWRHMWHandle, "WiFiNetworkGetChannel");
        BWRHMWScanAsync = (int (*)(BWRHMWDeviceRef, CFDictionaryRef, BWRHMWScanCallback, const void *))dlsym(BWRHMWHandle, "WiFiDeviceClientScanAsync");
        BWRHMWScanCancel = (void (*)(BWRHMWDeviceRef))dlsym(BWRHMWHandle, "WiFiDeviceClientScanCancel");
        BWRHMWRegisterLQM = (void (*)(BWRHMWDeviceRef, BWRHMWGenericCallback, const void *))dlsym(BWRHMWHandle, "WiFiDeviceClientRegisterLQMCallback");
    });
}

static void BWRHMWLQMCallbackFunction(BWRHMWDeviceRef device, CFDictionaryRef data, const void *object) {
    BWRHMWLQMEventCount += 1;
    long long eventRSSI = BWRHMWRSSIFromValue(data);
    dispatch_async(dispatch_get_main_queue(), ^{
        id monitor = BWRHMWActiveMonitor;
        SEL applySel = NSSelectorFromString(@"bwrh_applyDriverRSSI:");
        if (monitor && eventRSSI < 0 && eventRSSI > -200 && [monitor respondsToSelector:applySel]) {
            ((void (*)(id, SEL, long long))objc_msgSend)(monitor, applySel, eventRSSI);
            return;
        }
        SEL sel = NSSelectorFromString(@"sampleSignal");
        if (monitor && [monitor respondsToSelector:sel]) ((void (*)(id, SEL))objc_msgSend)(monitor, sel);
    });
}

static BOOL BWRHMWEnsureDevice(void) {
    BWRHMWResolveSymbols();
    if (!BWRHMWManagerCreate || !BWRHMWCopyDevices) return NO;
    if (!BWRHMWManager) {
        BWRHMWManager = BWRHMWManagerCreate(kCFAllocatorDefault, 0);
        if (!BWRHMWManager) return NO;
        if (BWRHMWSchedule) BWRHMWSchedule(BWRHMWManager, CFRunLoopGetMain(), kCFRunLoopCommonModes);
    }
    if (!BWRHMWDevice) {
        CFArrayRef devices = BWRHMWCopyDevices(BWRHMWManager);
        if (devices && CFArrayGetCount(devices) > 0) {
            BWRHMWDevice = (BWRHMWDeviceRef)CFRetain(CFArrayGetValueAtIndex(devices, 0));
        }
        if (devices) CFRelease(devices);
    }
    return BWRHMWDevice != NULL;
}

static BWRHMWDeviceRef BWRHMWDeviceForAirport(id airportController) {
    BWRHMWResolveSymbols();
    id wfInterface = BWRHWFInterfaceForAirport(airportController);
    SEL deviceSel = NSSelectorFromString(@"device");
    if (wfInterface && [wfInterface respondsToSelector:deviceSel]) {
        BWRHMWDeviceRef device = ((BWRHMWDeviceRef (*)(id, SEL))objc_msgSend)(wfInterface, deviceSel);
        if (device) return device;
    }
    return BWRHMWEnsureDevice() ? BWRHMWDevice : NULL;
}

static void BWRHMWUnregisterLQM(void) {
    if (BWRHMWLQMRegistered && BWRHMWLQMDevice && BWRHMWRegisterLQM) {
        BWRHMWRegisterLQM(BWRHMWLQMDevice, NULL, NULL);
    }
    BWRHMWLQMRegistered = NO;
    BWRHMWLQMDevice = NULL;
}

static void BWRHMWRegisterLQMForAirport(id airportController) {
    BWRHMWDeviceRef device = BWRHMWDeviceForAirport(airportController);
    if (!device || !BWRHMWRegisterLQM) return;
    if (BWRHMWLQMRegistered && BWRHMWLQMDevice == device) return;
    BWRHMWUnregisterLQM();
    BWRHMWRegisterLQM(device, BWRHMWLQMCallbackFunction, NULL);
    BWRHMWLQMRegistered = YES;
    BWRHMWLQMDevice = device;
}

static BWRHMWDeviceRef BWRHMWExistingDeviceForAirport(id airportController) {
    BWRHMWResolveSymbols();
    id wfInterface = BWRHWFInterfaceForAirport(airportController);
    SEL deviceSel = NSSelectorFromString(@"device");
    if (wfInterface && [wfInterface respondsToSelector:deviceSel]) {
        BWRHMWDeviceRef device = ((BWRHMWDeviceRef (*)(id, SEL))objc_msgSend)(wfInterface, deviceSel);
        if (device) return device;
    }
    return BWRHMWDevice;
}

static void BWRHMWCancelScanForAirport(id airportController) {
    BWRHMWDeviceRef device = BWRHMWExistingDeviceForAirport(airportController);
    if (device && BWRHMWScanCancel) BWRHMWScanCancel(device);
}

static void BWRHMWReleaseFallbackClientIfIdle(void) {
    if (BWRHMWActiveMonitor || BWRHMWActiveAnalyzer) return;
    BWRHMWUnregisterLQM();
    if (BWRHMWDevice) {
        CFRelease(BWRHMWDevice);
        BWRHMWDevice = NULL;
    }
    if (BWRHMWManager) {
        if (BWRHMWUnschedule) BWRHMWUnschedule(BWRHMWManager);
        CFRelease(BWRHMWManager);
        BWRHMWManager = NULL;
    }
}

static long long BWRHMWReadRSSIForAirport(id airportController) {
    BWRHMWDeviceRef device = BWRHMWDeviceForAirport(airportController);
    if (!device) return LLONG_MIN;
    if (BWRHMWCopyCurrentNetwork && BWRHMWNetworkGetRSSI) {
        BWRHMWNetworkRef network = BWRHMWCopyCurrentNetwork(device);
        if (network) {
            int direct = BWRHMWNetworkGetRSSI(network);
            CFRelease(network);
            if (direct < 0 && direct > -200) return direct;
        }
    }
    if (!BWRHMWDeviceCopyProperty) return LLONG_MIN;
    CFTypeRef value = BWRHMWDeviceCopyProperty(device, CFSTR("RSSI"));
    long long rssi = BWRHMWRSSIFromValue(value);
    if (value) CFRelease(value);
    return (rssi < 0 && rssi > -200) ? rssi : LLONG_MIN;
}

static __attribute__((unused)) long long BWRHMWReadRSSI(void) {
    if (!BWRHMWEnsureDevice() || !BWRHMWDeviceCopyProperty) return LLONG_MIN;
    CFTypeRef value = BWRHMWDeviceCopyProperty(BWRHMWDevice, CFSTR("RSSI"));
    long long rssi = BWRHMWRSSIFromValue(value);
    if (value) CFRelease(value);
    return (rssi < 0 && rssi > -200) ? rssi : LLONG_MIN;
}

static BWRHMobileWiFiNetwork *BWRHMWRecordFromNetwork(BWRHMWNetworkRef network) {
    if (!network) return nil;
    BWRHMobileWiFiNetwork *out = [BWRHMobileWiFiNetwork new];
    if (BWRHMWNetworkGetSSID) {
        CFStringRef ssid = BWRHMWNetworkGetSSID(network);
        if (ssid && CFGetTypeID(ssid) == CFStringGetTypeID()) out.ssid = (__bridge NSString *)ssid;
    }

    NSDictionary *record = nil;
    CFDictionaryRef copied = BWRHMWNetworkCopyRecord ? BWRHMWNetworkCopyRecord(network) : NULL;
    if (copied && CFGetTypeID(copied) == CFDictionaryGetTypeID()) record = (__bridge NSDictionary *)copied;

    id bssid = record[@"BSSID"];
    if (![bssid isKindOfClass:[NSString class]] && BWRHMWNetworkGetProperty) {
        CFTypeRef v = BWRHMWNetworkGetProperty(network, CFSTR("BSSID"));
        if (v && CFGetTypeID(v) == CFStringGetTypeID()) bssid = (__bridge NSString *)v;
    }
    if ([bssid isKindOfClass:[NSString class]]) out.bssid = bssid;

    id channel = nil;
    if (BWRHMWNetworkGetChannel) {
        int directChannel = BWRHMWNetworkGetChannel(network);
        if (directChannel > 0 && directChannel < 1000) channel = @(directChannel);
    }
    if (![channel isKindOfClass:[NSNumber class]]) channel = record[@"CHANNEL"];
    if (![channel isKindOfClass:[NSNumber class]] && BWRHMWNetworkGetProperty) {
        CFTypeRef v = BWRHMWNetworkGetProperty(network, CFSTR("CHANNEL"));
        if (v && CFGetTypeID(v) == CFNumberGetTypeID()) channel = (__bridge NSNumber *)v;
    }
    if ([channel isKindOfClass:[NSNumber class]]) out.channel = channel;

    long long rssi = LLONG_MIN;
    if (BWRHMWNetworkGetRSSI) {
        int directRSSI = BWRHMWNetworkGetRSSI(network);
        if (directRSSI < 0 && directRSSI > -200) rssi = directRSSI;
    }
    id rr = record[@"RSSI"];
    if (rssi == LLONG_MIN && [rr isKindOfClass:[NSNumber class]]) rssi = [rr longLongValue];
    if (rssi == LLONG_MIN && BWRHMWNetworkGetProperty) {
        CFTypeRef v = BWRHMWNetworkGetProperty(network, CFSTR("RSSI"));
        rssi = BWRHMWRSSIFromValue(v);
    }
    out.rssi = (rssi < 0 && rssi > -200) ? rssi : -999;

    id auth = record[@"AUTH_FLAGS"] ?: record[@"SECURITY"];
    out.secure = auth ? [auth integerValue] != 0 : YES;
    if (copied) CFRelease(copied);
    return out;
}

static BWRHMobileWiFiNetwork *BWRHMWCurrentNetworkRecordForAirport(id airportController) {
    BWRHMWDeviceRef device = BWRHMWDeviceForAirport(airportController);
    if (!device || !BWRHMWCopyCurrentNetwork) return nil;
    BWRHMWNetworkRef network = BWRHMWCopyCurrentNetwork(device);
    if (!network) return nil;
    BWRHMobileWiFiNetwork *record = BWRHMWRecordFromNetwork(network);
    CFRelease(network);
    return record;
}

static __attribute__((unused)) BWRHMobileWiFiNetwork *BWRHMWCurrentNetworkRecord(void) {
    if (!BWRHMWEnsureDevice() || !BWRHMWCopyCurrentNetwork) return nil;
    BWRHMWNetworkRef network = BWRHMWCopyCurrentNetwork(BWRHMWDevice);
    if (!network) return nil;
    BWRHMobileWiFiNetwork *record = BWRHMWRecordFromNetwork(network);
    CFRelease(network);
    return record;
}

static NSArray *BWRHMWRecordsFromScan(CFArrayRef results) {
    if (!results) return @[];
    NSMutableArray *out = [NSMutableArray array];
    CFIndex count = CFArrayGetCount(results);
    for (CFIndex i = 0; i < count; i++) {
        BWRHMWNetworkRef network = (BWRHMWNetworkRef)CFArrayGetValueAtIndex(results, i);
        BWRHMobileWiFiNetwork *record = BWRHMWRecordFromNetwork(network);
        if (record.channel.integerValue > 0) [out addObject:record];
    }
    return out;
}

static void BWRHMWScanCallbackFunction(BWRHMWDeviceRef device, CFArrayRef results, int error, const void *object) {
    NSArray *records = BWRHMWRecordsFromScan(results);
    dispatch_async(dispatch_get_main_queue(), ^{
        id analyzer = BWRHMWActiveAnalyzer;
        SEL sel = NSSelectorFromString(@"bwrh_applyMobileWiFiScanResults:error:");
        if (analyzer && [analyzer respondsToSelector:sel]) {
            ((void (*)(id, SEL, id, NSInteger))objc_msgSend)(analyzer, sel, records, (NSInteger)error);
        }
    });
}

static BOOL BWRHMWStartScanForAirport(id airportController) {
    BWRHMWResolveSymbols();
    BWRHMWDeviceRef device = BWRHMWDeviceForAirport(airportController);
    if (!device || !BWRHMWScanAsync) return NO;
    NSDictionary *params = @{
        @"SCAN_TYPE": @1,
        @"SCAN_NUM_SCANS": @1,
        @"SCAN_RSSI_THRESHOLD": @(-170),
        @"SCAN_MERGE": @14,
        @"SCAN_MAXAGE": @0
    };
    int rc = BWRHMWScanAsync(device, (__bridge CFDictionaryRef)params, BWRHMWScanCallbackFunction, NULL);
    return rc == 0;
}

static __attribute__((unused)) BOOL BWRHMWStartScan(void) {
    return BWRHMWStartScanForAirport(nil);
}

#pragma mark - Signal monitor

@interface BWRHSignalGraphView : UIView
@property (nonatomic, copy) NSArray<NSNumber *> *samples;
@end

@implementation BWRHSignalGraphView

- (void)setSamples:(NSArray<NSNumber *> *)samples {
    _samples = [samples copy];
    [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect {
    [super drawRect:rect];
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    if (!ctx) return;

    CGRect bounds = CGRectInset(self.bounds, 12.0, 12.0);
    [[UIColor secondarySystemBackgroundColor] setFill];
    UIBezierPath *background = [UIBezierPath bezierPathWithRoundedRect:self.bounds cornerRadius:14.0];
    [background fill];

    [[UIColor separatorColor] setStroke];
    CGContextSetLineWidth(ctx, 0.5);
    for (NSInteger i = 0; i <= 4; i++) {
        CGFloat y = CGRectGetMinY(bounds) + (CGRectGetHeight(bounds) * i / 4.0);
        CGContextMoveToPoint(ctx, CGRectGetMinX(bounds), y);
        CGContextAddLineToPoint(ctx, CGRectGetMaxX(bounds), y);
    }
    CGContextStrokePath(ctx);

    if (self.samples.count < 2) return;

    UIBezierPath *line = [UIBezierPath bezierPath];
    line.lineWidth = 2.5;
    NSInteger count = self.samples.count;
    for (NSInteger i = 0; i < count; i++) {
        double rssi = self.samples[i].doubleValue;
        double normalized = (rssi + 100.0) / 70.0;
        normalized = MAX(0.0, MIN(1.0, normalized));
        CGFloat x = CGRectGetMinX(bounds) + (count <= 1 ? 0 : CGRectGetWidth(bounds) * i / (count - 1.0));
        CGFloat y = CGRectGetMaxY(bounds) - CGRectGetHeight(bounds) * normalized;
        if (i == 0) [line moveToPoint:CGPointMake(x, y)];
        else [line addLineToPoint:CGPointMake(x, y)];
    }
    [[UIColor systemBlueColor] setStroke];
    [line stroke];
}

@end

@interface BWRHSignalMonitorController : UIViewController
@property (nonatomic, weak) UIViewController *airportController;
@property (nonatomic, strong) id fallbackNetwork;
@property (nonatomic, strong) NSMutableArray<NSNumber *> *samples;
@property (nonatomic, strong) dispatch_source_t sampleTimer;
@property (nonatomic) NSUInteger sampleCount;
@property (nonatomic, copy) NSString *lastSampleSource;
@property (nonatomic, strong) UILabel *valueLabel;
@property (nonatomic, strong) UILabel *qualityLabel;
@property (nonatomic, strong) UILabel *metaLabel;
@property (nonatomic, strong) BWRHSignalGraphView *graphView;
- (instancetype)initWithAirportController:(UIViewController *)airport network:(id)network;
@end

@implementation BWRHSignalMonitorController

- (instancetype)initWithAirportController:(UIViewController *)airport network:(id)network {
    if ((self = [super init])) {
        _airportController = airport;
        _fallbackNetwork = network;
        _samples = [NSMutableArray array];
        self.title = BWRHT(@"Monitor de señal", @"Signal monitor");
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

    self.valueLabel = [[UILabel alloc] init];
    self.valueLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.valueLabel.font = [UIFont monospacedDigitSystemFontOfSize:42 weight:UIFontWeightSemibold];
    self.valueLabel.textAlignment = NSTextAlignmentCenter;
    self.valueLabel.text = @"— dBm";

    self.qualityLabel = [[UILabel alloc] init];
    self.qualityLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.qualityLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    self.qualityLabel.textAlignment = NSTextAlignmentCenter;
    self.qualityLabel.textColor = [UIColor secondaryLabelColor];

    self.metaLabel = [[UILabel alloc] init];
    self.metaLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.metaLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    self.metaLabel.textAlignment = NSTextAlignmentCenter;
    self.metaLabel.numberOfLines = 0;
    self.metaLabel.textColor = [UIColor secondaryLabelColor];

    self.graphView = [[BWRHSignalGraphView alloc] init];
    self.graphView.translatesAutoresizingMaskIntoConstraints = NO;

    UILabel *footer = [[UILabel alloc] init];
    footer.translatesAutoresizingMaskIntoConstraints = NO;
    footer.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    footer.textColor = [UIColor tertiaryLabelColor];
    footer.textAlignment = NSTextAlignmentCenter;
    footer.numberOfLines = 0;
    footer.text = BWRHT(@"El escaneo solo permanece activo mientras esta pantalla está abierta. Al salir, el temporizador se detiene automáticamente.",
                        @"Scanning is active only while this screen is open. The timer stops automatically when you leave.");

    [self.view addSubview:self.valueLabel];
    [self.view addSubview:self.qualityLabel];
    [self.view addSubview:self.metaLabel];
    [self.view addSubview:self.graphView];
    [self.view addSubview:footer];

    UILayoutGuide *guide = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [self.valueLabel.topAnchor constraintEqualToAnchor:guide.topAnchor constant:28],
        [self.valueLabel.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:20],
        [self.valueLabel.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-20],
        [self.qualityLabel.topAnchor constraintEqualToAnchor:self.valueLabel.bottomAnchor constant:4],
        [self.qualityLabel.leadingAnchor constraintEqualToAnchor:self.valueLabel.leadingAnchor],
        [self.qualityLabel.trailingAnchor constraintEqualToAnchor:self.valueLabel.trailingAnchor],
        [self.metaLabel.topAnchor constraintEqualToAnchor:self.qualityLabel.bottomAnchor constant:10],
        [self.metaLabel.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:24],
        [self.metaLabel.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-24],
        [self.graphView.topAnchor constraintEqualToAnchor:self.metaLabel.bottomAnchor constant:24],
        [self.graphView.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:18],
        [self.graphView.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-18],
        [self.graphView.heightAnchor constraintEqualToConstant:250],
        [footer.topAnchor constraintEqualToAnchor:self.graphView.bottomAnchor constant:18],
        [footer.leadingAnchor constraintEqualToAnchor:guide.leadingAnchor constant:28],
        [footer.trailingAnchor constraintEqualToAnchor:guide.trailingAnchor constant:-28]
    ]];
}

- (id)currentNetwork {
    id network = BWRHMsgObject(self.airportController, @"currentNetwork");
    return BWRHResolvedCurrentNetwork(network ?: self.fallbackNetwork);
}

- (void)bwrh_renderRSSI:(long long)rssi source:(NSString *)source {
    self.sampleCount += 1;
    self.lastSampleSource = source ?: @"�";
    if (rssi != LLONG_MIN) {
        [self.samples addObject:@(rssi)];
        while (self.samples.count > 60) [self.samples removeObjectAtIndex:0];
        self.valueLabel.text = [NSString stringWithFormat:@"%lld dBm", rssi];
        self.qualityLabel.text = BWRHSignalQuality(rssi);
        self.graphView.samples = [self.samples copy];
    } else {
        self.valueLabel.text = @"� dBm";
        self.qualityLabel.text = BWRHT(@"RSSI no disponible", @"RSSI unavailable");
    }

    id fallback = [self currentNetwork];
    id liveNetwork = BWRHMWCurrentNetworkRecordForAirport(self.airportController) ?: BWRHLiveScanResultForAirport(self.airportController) ?: fallback;
    NSNumber *channel = BWRHChannelNumber(liveNetwork);
    NSString *band = BWRHBandDescription(liveNetwork);
    NSString *bssid = BWRHBSSID(liveNetwork);
    NSMutableArray *meta = [NSMutableArray array];
    [meta addObject:self.lastSampleSource ?: @"�"];
    [meta addObject:[NSString stringWithFormat:BWRHT(@"muestra %lu", @"sample %lu"), (unsigned long)self.sampleCount]];
    [meta addObject:[NSString stringWithFormat:@"LQM MW %lu / WK %lu", (unsigned long)BWRHMWLQMEventCount, (unsigned long)BWRHWiFiKitLQEventCount]];
    if (band.length) [meta addObject:band];
    if (channel) [meta addObject:[NSString stringWithFormat:@"%@ %@", BWRHT(@"Canal", @"Channel"), channel]];
    if (bssid.length) [meta addObject:bssid];
    self.metaLabel.text = [meta componentsJoinedByString:@" � "];
}

- (void)bwrh_applyDriverRSSI:(long long)rssi {
    if (!self.view.window || rssi >= 0 || rssi <= -200) return;
    [self bwrh_renderRSSI:rssi source:@"MobileWiFi LQM"];
}

- (void)bwrh_applyWiFiKitLQMRSSI:(long long)rssi {
    if (!self.view.window || rssi >= 0 || rssi <= -200) return;
    [self bwrh_renderRSSI:rssi source:@"WiFiKit LQ event"];
}

- (void)sampleSignal {
    id fallback = [self currentNetwork];
    long long rssi = BWRHMWReadRSSIForAirport(self.airportController);
    BOOL mobile = (rssi != LLONG_MIN);
    if (rssi == LLONG_MIN) rssi = BWRHLiveRSSIForAirport(self.airportController, fallback);
    [self bwrh_renderRSSI:rssi source:(mobile ? @"MobileWiFi directo" : @"WiFiKit fallback")];
}

- (void)startMonitoring {
    [self stopMonitoring];
    BWRHMWActiveMonitor = self;
    BWRHMWRegisterLQMForAirport(self.airportController);
    [self sampleSignal];

    NSTimeInterval interval = BWRHBool(@"efficientMode", YES) ? 3.0 : 1.0;
    uint64_t intervalNS = (uint64_t)(interval * NSEC_PER_SEC);
    uint64_t leewayNS = (uint64_t)(0.25 * NSEC_PER_SEC);
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    self.sampleTimer = timer;
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, intervalNS), intervalNS, leewayNS);
    __weak typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(timer, ^{
        __strong typeof(weakSelf) self = weakSelf;
        if (!self || !self.view.window || self.sampleTimer != timer) return;
        [self sampleSignal];
    });
    dispatch_resume(timer);
}

- (void)stopMonitoring {
    dispatch_source_t timer = self.sampleTimer;
    self.sampleTimer = nil;
    if (timer) dispatch_source_cancel(timer);
    if (BWRHMWActiveMonitor == self) BWRHMWActiveMonitor = nil;
    BWRHMWUnregisterLQM();
    BWRHMWReleaseFallbackClientIfIdle();
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self startMonitoring];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self stopMonitoring];
}

- (void)dealloc {
    [self stopMonitoring];
}

@end

#pragma mark - Channel analyzer

@interface BWRHChannelAnalyzerController : UITableViewController
@property (nonatomic, weak) UIViewController *airportController;
@property (nonatomic, strong) id currentNetwork;
@property (nonatomic, copy) NSArray *networks;
@property (nonatomic, copy) NSArray<NSDictionary *> *rows24;
@property (nonatomic, copy) NSArray<NSDictionary *> *rows5;
@property (nonatomic) BOOL mobileWiFiScanSucceeded;
@property (nonatomic) NSInteger mobileWiFiScanError;
@property (nonatomic, copy) NSString *scanSource;
@property (nonatomic) BOOL analyzerVisible;
@property (nonatomic) NSUInteger scanGeneration;
- (instancetype)initWithAirportController:(UIViewController *)airport currentNetwork:(id)network;
@end

@implementation BWRHChannelAnalyzerController

- (instancetype)initWithAirportController:(UIViewController *)airport currentNetwork:(id)network {
    if ((self = [super initWithStyle:UITableViewStyleInsetGrouped])) {
        _airportController = airport;
        _currentNetwork = network;
        _networks = BWRHLatestScanNetworks(airport);
        _scanSource = @"Inicial";
        self.title = BWRHT(@"Analizador de canales", @"Channel analyzer");
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self action:@selector(refreshScan)];
    self.networks = BWRHLatestScanNetworks(self.airportController);
    [self rebuildRows];
}

- (void)refreshScan {
    if (!self.analyzerVisible && self.view.window == nil) return;
    self.scanGeneration += 1;
    NSUInteger generation = self.scanGeneration;
    self.navigationItem.rightBarButtonItem.enabled = NO;
    BWRHMWActiveAnalyzer = self;
    self.mobileWiFiScanSucceeded = NO;
    self.mobileWiFiScanError = 0;
    self.scanSource = @"MobileWiFi pendiente";
    if (BWRHMWStartScanForAirport(self.airportController)) {
        __weak typeof(self) weakSelf = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self || !self.analyzerVisible || self.scanGeneration != generation || self.navigationItem.rightBarButtonItem.enabled) return;
            self.scanSource = @"WiFiKit fallback (timeout)";
            BWRHRequestFreshScan(self.airportController);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                if (!self.analyzerVisible || self.scanGeneration != generation) return;
                self.networks = BWRHLatestScanNetworks(self.airportController);
                [self rebuildRows];
                [self.tableView reloadData];
                self.navigationItem.rightBarButtonItem.enabled = YES;
            });
        });
        return;
    }

    self.scanSource = @"WiFiKit fallback (MobileWiFi no disponible)";
    BWRHRequestFreshScan(self.airportController);
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.8 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        __strong typeof(weakSelf) self = weakSelf;
        if (!self || !self.analyzerVisible || self.scanGeneration != generation) return;
        self.networks = BWRHLatestScanNetworks(self.airportController);
        [self rebuildRows];
        [self.tableView reloadData];
        self.navigationItem.rightBarButtonItem.enabled = YES;
    });
}

- (void)bwrh_applyMobileWiFiScanResults:(NSArray *)results error:(NSInteger)error {
    if (!self.analyzerVisible || BWRHMWActiveAnalyzer != self) return;
    self.mobileWiFiScanError = error;
    self.mobileWiFiScanSucceeded = results.count > 0;
    if (results.count) {
        self.scanSource = @"MobileWiFi directo";
        self.networks = results;
    } else {
        self.scanSource = [NSString stringWithFormat:@"WiFiKit fallback (MW error %ld)", (long)error];
        self.networks = BWRHLatestScanNetworks(self.airportController);
    }
    [self rebuildRows];
    [self.tableView reloadData];
    self.navigationItem.rightBarButtonItem.enabled = YES;
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    self.analyzerVisible = YES;
    BWRHMWActiveAnalyzer = self;
    if ([self.scanSource isEqualToString:@"Inicial"]) [self refreshScan];
}

- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    self.analyzerVisible = NO;
    self.scanGeneration += 1;
    BWRHMWCancelScanForAirport(self.airportController);
    self.navigationItem.rightBarButtonItem.enabled = YES;
    if (BWRHMWActiveAnalyzer == self) BWRHMWActiveAnalyzer = nil;
    BWRHMWReleaseFallbackClientIfIdle();
}

- (void)dealloc {
    BWRHMWCancelScanForAirport(self.airportController);
    if (BWRHMWActiveAnalyzer == self) BWRHMWActiveAnalyzer = nil;
    BWRHMWReleaseFallbackClientIfIdle();
}

- (NSDictionary *)rowFor24Target:(NSInteger)target {
    NSInteger same = 0;
    NSInteger load = 0;
    long long strongest = -999;
    for (id network in self.networks) {
        NSNumber *channelNumber = BWRHChannelNumber(network);
        NSInteger channel = channelNumber.integerValue;
        if (channel < 1 || channel > 14) continue;
        NSInteger distance = labs(channel - target);
        if (distance == 0) same++;
        if (distance <= 4) load += (5 - distance);
        long long rssi = BWRHMsgLongLong(network, @"rssi", -999);
        if (distance <= 4 && rssi > strongest) strongest = rssi;
    }
    NSString *detail = strongest > -999
        ? [NSString stringWithFormat:BWRHT(@"Carga %ld · %ld AP · mejor %lld dBm", @"Load %ld · %ld AP · best %lld dBm"), (long)load, (long)same, strongest]
        : [NSString stringWithFormat:BWRHT(@"Carga %ld · %ld AP", @"Load %ld · %ld AP"), (long)load, (long)same];
    return @{ @"channel": @(target), @"detail": detail, @"load": @(load) };
}

- (void)rebuildRows {
    self.rows24 = @[[self rowFor24Target:1], [self rowFor24Target:6], [self rowFor24Target:11]];

    NSMutableDictionary<NSNumber *, NSMutableDictionary *> *channels = [NSMutableDictionary dictionary];
    for (id network in self.networks) {
        NSNumber *channelNumber = BWRHChannelNumber(network);
        NSInteger channel = channelNumber.integerValue;
        if (channel < 32 || channel > 177) continue;
        NSMutableDictionary *entry = channels[@(channel)];
        if (!entry) {
            entry = [@{ @"channel": @(channel), @"count": @0, @"strongest": @(-999) } mutableCopy];
            channels[@(channel)] = entry;
        }
        entry[@"count"] = @([entry[@"count"] integerValue] + 1);
        long long rssi = BWRHMsgLongLong(network, @"rssi", -999);
        if (rssi > [entry[@"strongest"] longLongValue]) entry[@"strongest"] = @(rssi);
    }

    NSArray *keys = [[channels allKeys] sortedArrayUsingSelector:@selector(compare:)];
    NSMutableArray *rows = [NSMutableArray array];
    for (NSNumber *key in keys) {
        NSDictionary *entry = channels[key];
        long long strongest = [entry[@"strongest"] longLongValue];
        NSString *detail = strongest > -999
            ? [NSString stringWithFormat:BWRHT(@"%@ AP · mejor %lld dBm", @"%@ AP · best %lld dBm"), entry[@"count"], strongest]
            : [NSString stringWithFormat:BWRHT(@"%@ AP", @"%@ AP"), entry[@"count"]];
        [rows addObject:@{ @"channel": key, @"detail": detail }];
    }
    self.rows5 = rows;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return section == 0 ? self.rows24.count : self.rows5.count;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return section == 0 ? @"2.4 GHz · 1 / 6 / 11" : @"5 GHz";
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    if (section == 0 && self.rows24.count) {
        NSDictionary *best = [self.rows24 sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *a, NSDictionary *b) {
            return [a[@"load"] compare:b[@"load"]];
        }].firstObject;
        return [NSString stringWithFormat:BWRHT(@"Entre 1/6/11, el canal %@ tiene la menor carga observada. La carga es una estimación basada en redes detectadas y solapamiento de canales.",
                                               @"Among 1/6/11, channel %@ has the lowest observed load. Load is an estimate based on detected networks and channel overlap."), best[@"channel"]];
    }
    if (section == 1 && self.rows5.count == 0) {
        return [NSString stringWithFormat:BWRHT(@"No se detectaron redes de 5 GHz. Fuente: %@ · registros: %ld · error MW: %ld. MobileWiFi se solicita sin límite de canal y con umbral -170 dBm.", @"No 5 GHz networks were detected. Source: %@ · records: %ld · MW error: %ld. MobileWiFi is requested with no channel limit and a -170 dBm threshold."), self.scanSource ?: @"—", (long)self.networks.count, (long)self.mobileWiFiScanError];
    }
    if (section == 1) return [NSString stringWithFormat:BWRHT(@"Fuente: %@ · %ld registros analizados.", @"Source: %@ · %ld records analyzed."), self.scanSource ?: @"—", (long)self.networks.count];
    return nil;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"channel"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"channel"];
    NSDictionary *row = indexPath.section == 0 ? self.rows24[indexPath.row] : self.rows5[indexPath.row];
    cell.textLabel.text = [NSString stringWithFormat:BWRHT(@"Canal %@", @"Channel %@"), row[@"channel"]];
    cell.detailTextLabel.text = row[@"detail"];
    cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
    cell.detailTextLabel.minimumScaleFactor = 0.65;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;

    NSNumber *currentChannel = BWRHChannelNumber(self.currentNetwork);
    cell.accessoryType = currentChannel.integerValue == [row[@"channel"] integerValue] ? UITableViewCellAccessoryCheckmark : UITableViewCellAccessoryNone;
    return cell;
}

@end

#pragma mark - Quick diagnostics

@interface BWRHDiagnosticsController : UITableViewController
@property (nonatomic, strong) id network;
@property (nonatomic, strong) id config;
@property (nonatomic, strong) NSMutableArray<NSMutableDictionary *> *rows;
@property (nonatomic) BOOL running;
- (instancetype)initWithNetwork:(id)network config:(id)config;
@end

@implementation BWRHDiagnosticsController

- (instancetype)initWithNetwork:(id)network config:(id)config {
    if ((self = [super initWithStyle:UITableViewStyleInsetGrouped])) {
        _network = network;
        _config = config;
        self.title = BWRHT(@"Diagnóstico Wi‑Fi", @"Wi-Fi diagnostics");
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:BWRHT(@"Repetir", @"Run again") style:UIBarButtonItemStylePlain target:self action:@selector(runDiagnostics)];
    [self resetRows];
    [self runDiagnostics];
}

- (void)resetRows {
    long long rssi = BWRHMsgLongLong(self.network, @"rssi", LLONG_MIN);
    NSString *signal = (rssi != LLONG_MIN && rssi < 0 && rssi > -200)
        ? [NSString stringWithFormat:@"%lld dBm · %@", rssi, BWRHSignalQuality(rssi)]
        : BWRHT(@"No disponible", @"Unavailable");
    NSString *ip = BWRHStringValue(BWRHMsgObject(self.config, @"ipv4Address")) ?: BWRHT(@"Sin IPv4", @"No IPv4");
    NSString *router = BWRHStringValue(BWRHMsgObject(self.config, @"ipv4RouterAddress")) ?: BWRHT(@"No disponible", @"Unavailable");
    id dns = BWRHMsgObject(self.config, @"dnsServerAddresses");
    NSString *dnsConfigured = ([dns isKindOfClass:[NSArray class]] && [dns count]) ? [(NSArray *)dns componentsJoinedByString:@", "] : BWRHT(@"No configurado", @"Not configured");

    self.rows = [@[
        [@{ @"label": BWRHT(@"Señal", @"Signal"), @"value": signal } mutableCopy],
        [@{ @"label": BWRHT(@"Dirección IP", @"IP address"), @"value": ip } mutableCopy],
        [@{ @"label": BWRHT(@"Router", @"Router"), @"value": router } mutableCopy],
        [@{ @"label": @"DNS", @"value": dnsConfigured } mutableCopy],
        [@{ @"label": BWRHT(@"Resolución DNS", @"DNS resolution"), @"value": BWRHT(@"Pendiente…", @"Pending…") } mutableCopy],
        [@{ @"label": BWRHT(@"Acceso a Internet", @"Internet access"), @"value": BWRHT(@"Pendiente…", @"Pending…") } mutableCopy]
    ] mutableCopy];
    [self.tableView reloadData];
}

- (void)updateRow:(NSInteger)index value:(NSString *)value {
    if (index < 0 || index >= (NSInteger)self.rows.count) return;
    self.rows[index][@"value"] = value ?: @"";
    if (self.isViewLoaded) {
        NSIndexPath *path = [NSIndexPath indexPathForRow:index inSection:0];
        [self.tableView reloadRowsAtIndexPaths:@[path] withRowAnimation:UITableViewRowAnimationNone];
    }
}

- (void)runDiagnostics {
    if (self.running) return;
    self.running = YES;
    self.navigationItem.rightBarButtonItem.enabled = NO;
    [self resetRows];

    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *result = NULL;
        int rc = getaddrinfo("captive.apple.com", "443", &hints, &result);
        if (result) freeaddrinfo(result);
        double ms = (CFAbsoluteTimeGetCurrent() - start) * 1000.0;
        NSString *value = rc == 0
            ? [NSString stringWithFormat:BWRHT(@"Correcto · %.0f ms", @"OK · %.0f ms"), ms]
            : [NSString stringWithFormat:BWRHT(@"Error (%d)", @"Error (%d)"), rc];
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf updateRow:4 value:value];
        });
    });

    NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.timeoutIntervalForRequest = 8.0;
    configuration.timeoutIntervalForResource = 10.0;
    NSURLSession *session = [NSURLSession sessionWithConfiguration:configuration];
    NSURL *url = [NSURL URLWithString:@"https://captive.apple.com/hotspot-detect.html"];
    CFAbsoluteTime requestStart = CFAbsoluteTimeGetCurrent();
    NSURLSessionDataTask *task = [session dataTaskWithURL:url completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        double ms = (CFAbsoluteTimeGetCurrent() - requestStart) * 1000.0;
        NSString *value = nil;
        if (error) {
            value = [NSString stringWithFormat:BWRHT(@"Sin acceso · %@", @"No access · %@"), error.localizedDescription ?: @""];
        } else {
            NSInteger status = [(NSHTTPURLResponse *)response statusCode];
            NSString *body = data.length ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : @"";
            BOOL successPage = status >= 200 && status < 300 && [body rangeOfString:@"Success" options:NSCaseInsensitiveSearch].location != NSNotFound;
            if (successPage) {
                value = [NSString stringWithFormat:BWRHT(@"Correcto · %.0f ms", @"OK · %.0f ms"), ms];
            } else if (status >= 200 && status < 400) {
                value = [NSString stringWithFormat:BWRHT(@"Posible portal cautivo · HTTP %ld", @"Possible captive portal · HTTP %ld"), (long)status];
            } else {
                value = [NSString stringWithFormat:@"HTTP %ld · %.0f ms", (long)status, ms];
            }
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf updateRow:5 value:value];
            weakSelf.running = NO;
            weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
        });
        [session finishTasksAndInvalidate];
    }];
    [task resume];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return 1; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { return self.rows.count; }

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    return BWRHT(@"El diagnóstico se ejecuta solo bajo demanda. No deja pruebas de red ni procesos ejecutándose al salir de esta pantalla.",
                 @"Diagnostics run only on demand. No network tests or processes remain running after you leave this screen.");
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"diag"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"diag"];
    NSDictionary *row = self.rows[indexPath.row];
    cell.textLabel.text = row[@"label"];
    cell.detailTextLabel.text = row[@"value"];
    cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
    cell.detailTextLabel.minimumScaleFactor = 0.6;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
}

@end

#pragma mark - Safe BetterWiFi details screen

@interface BWRHConnectedDetailsController : UITableViewController
@property (nonatomic, weak) UIViewController *sourceController;
@property (nonatomic, weak) UIViewController *airportController;
@property (nonatomic, copy) NSArray<NSDictionary *> *items;
- (instancetype)initWithSourceController:(UIViewController *)source airportController:(UIViewController *)airport;
@end

@implementation BWRHConnectedDetailsController

- (instancetype)initWithSourceController:(UIViewController *)source airportController:(UIViewController *)airport {
    if ((self = [super initWithStyle:UITableViewStyleInsetGrouped])) {
        _sourceController = source;
        _airportController = airport;
        _items = BWRHCurrentInfoItems(source);
        self.title = @"BetterWiFi RH";
    }
    return self;
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    self.items = BWRHCurrentInfoItems(self.sourceController);
    [self.tableView reloadData];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return 1; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { return self.items.count; }

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    id network = BWRHResolvedCurrentNetwork(BWRHCurrentDetailsNetwork(self.sourceController));
    NSString *ssid = BWRHNetworkSSID(network);
    return ssid.length ? ssid : BWRHT(@"Red conectada", @"Connected network");
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    return BWRHT(@"Esta pantalla es independiente de las secciones internas de WiFiKit para evitar modificar la estructura privada de la pantalla de Apple.",
                 @"This screen is independent from WiFiKit's internal sections to avoid modifying Apple's private table structure.");
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    NSDictionary *item = self.items[indexPath.row];
    BOOL actionable = item[@"action"] != nil;
    NSString *identifier = actionable ? @"BWRHSafeToolCell" : @"BWRHSafeInfoCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:identifier];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:identifier];
    cell.textLabel.text = item[@"label"];
    cell.detailTextLabel.text = item[@"value"];
    cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
    cell.detailTextLabel.minimumScaleFactor = 0.6;
    cell.accessoryType = actionable ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
    cell.selectionStyle = actionable ? UITableViewCellSelectionStyleDefault : UITableViewCellSelectionStyleNone;
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    NSDictionary *item = self.items[indexPath.row];
    NSString *action = item[@"action"];
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if (!action.length) return;

    id network = BWRHResolvedCurrentNetwork(BWRHCurrentDetailsNetwork(self.sourceController));
    UIViewController *destination = nil;
    if ([action isEqualToString:@"signal"]) {
        destination = [[BWRHSignalMonitorController alloc] initWithAirportController:self.airportController network:network];
    } else if ([action isEqualToString:@"channels"]) {
        destination = [[BWRHChannelAnalyzerController alloc] initWithAirportController:self.airportController currentNetwork:network];
    } else if ([action isEqualToString:@"diagnostics"]) {
        destination = [[BWRHDiagnosticsController alloc] initWithNetwork:network config:BWRHMsgObject(self.sourceController, @"config")];
    }
    if (destination) [self.navigationController pushViewController:destination animated:YES];
}

@end

#pragma mark - Hooks

%hook WFAirportViewController

- (void)viewDidLoad {
    %orig;
    BWRHConfigureRefreshControl((UIViewController *)self);
}

- (void)viewWillAppear:(BOOL)animated {
    %orig(animated);
    BWRHActiveAirportController = (UIViewController *)self;
    BWRHApplyListPreferences((id)self, NO);
}

- (void)viewDidAppear:(BOOL)animated {
    %orig(animated);
    BWRHActiveAirportController = (UIViewController *)self;
    BWRHApplyListPreferences((id)self, YES);
}

- (void)viewDidDisappear:(BOOL)animated {
    if (BWRHActiveAirportController == (UIViewController *)self) BWRHActiveAirportController = nil;
    %orig(animated);
}

- (id)_currentNetworkCell {
    id cell = %orig;
    if (BWRHBool(@"enabled", YES) && BWRHBool(@"showCurrentSummary", YES)) {
        id network = BWRHResolvedCurrentNetwork(BWRHMsgObject((id)self, @"currentNetwork"));
        BWRHDecorateCell(cell, network);
    }
    return cell;
}

- (void)setCurrentNetworkSubtitle:(NSString *)subtitle {
    if (!BWRHBool(@"enabled", YES) || !BWRHBool(@"showCurrentSummary", YES)) {
        %orig(subtitle);
        return;
    }

    id network = BWRHResolvedCurrentNetwork(BWRHMsgObject((id)self, @"currentNetwork"));
    NSString *details = BWRHDetailsForNetwork(network);
    if (!details.length) {
        %orig(subtitle);
        return;
    }

    NSString *previous = objc_getAssociatedObject((id)self, BWRHCurrentSubtitleDetailsKey);
    NSString *base = subtitle ?: @"";
    if (previous.length) {
        if ([base isEqualToString:previous]) base = @"";
        NSString *suffix = [NSString stringWithFormat:@" · %@", previous];
        if ([base hasSuffix:suffix]) base = [base substringToIndex:base.length - suffix.length];
    }
    NSString *combined = base.length ? [NSString stringWithFormat:@"%@ · %@", base, details] : details;
    objc_setAssociatedObject((id)self, BWRHCurrentSubtitleDetailsKey, details, OBJC_ASSOCIATION_COPY_NONATOMIC);
    %orig(combined);
}

- (id)_tableCellForNetwork:(id)network tableView:(id)tableView indexPath:(id)indexPath {
    id cell = %orig(network, tableView, indexPath);
    BWRHDecorateCell(cell, network);
    return cell;
}

- (void)setNetworks:(id)networks {
    %orig(BWRHProcessNetworkCollection(networks));
}

- (void)setAllNetworks:(NSSet *)networks {
    %orig((NSSet *)BWRHProcessNetworkCollection(networks));
}

- (void)setInfraNetworks:(NSArray *)networks {
    %orig(BWRHProcessNetworks(networks));
    if (BWRHBool(@"enabled", YES) && BWRHBool(@"showCurrentSummary", YES)) {
        NSString *subtitle = BWRHStringValue(BWRHMsgObject((id)self, @"currentNetworkSubtitle")) ?: @"";
        ((void (*)(id, SEL, id))objc_msgSend)((id)self, NSSelectorFromString(@"setCurrentNetworkSubtitle:"), subtitle);
    }
}

- (void)setCurrentNetwork:(id)network previousNetwork:(id)previous reason:(unsigned long long)reason {
    %orig(network, previous, reason);
    if (BWRHBool(@"enabled", YES) && BWRHBool(@"showCurrentSummary", YES)) {
        NSString *subtitle = BWRHStringValue(BWRHMsgObject((id)self, @"currentNetworkSubtitle")) ?: @"";
        ((void (*)(id, SEL, id))objc_msgSend)((id)self, NSSelectorFromString(@"setCurrentNetworkSubtitle:"), subtitle);
    }
}

%new
- (void)bwrh_pullToRefresh:(UIRefreshControl *)sender {
    if ([(id)self respondsToSelector:NSSelectorFromString(@"refresh")]) {
        ((void (*)(id, SEL))objc_msgSend)((id)self, NSSelectorFromString(@"refresh"));
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(8.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (sender.refreshing) [sender endRefreshing];
    });
}

- (void)setScanning:(BOOL)scanning {
    %orig(scanning);
    if (!scanning) {
        UIRefreshControl *refresh = objc_getAssociatedObject((id)self, BWRHRefreshControlKey);
        if (refresh.refreshing) [refresh endRefreshing];
    }
}

%end

%hook WFNetworkListController

- (void)setLatestLinkQuality:(id)quality {
    %orig(quality);
    long long rssi = BWRHMsgLongLong(quality, @"rssi", LLONG_MIN);
    if (rssi < 0 && rssi > -200) {
        BWRHWiFiKitLQEventCount += 1;
        id monitor = BWRHMWActiveMonitor;
        SEL sel = NSSelectorFromString(@"bwrh_applyWiFiKitLQMRSSI:");
        if (monitor && [monitor respondsToSelector:sel]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                ((void (*)(id, SEL, long long))objc_msgSend)(monitor, sel, rssi);
            });
        }
    }
}

- (void)setNetworks:(id)networks {
    BWRHStoreNetworkCollection(networks);
    %orig(networks);
}

- (void)scanManager:(id)manager updatedPartialResults:(id)results {
    BWRHStoreNetworkCollection(results);
    %orig(manager, results);
}

- (void)scanManagerScanningDidFinish:(id)manager withResults:(id)results error:(id)error {
    BWRHStoreNetworkCollection(results);
    %orig(manager, results, error);
}

- (id)scanManager:(id)manager filterScanResults:(id)results {
    BWRHStoreNetworkCollection(results);
    return %orig(manager, results);
}

- (void)scanManager:(id)manager didFinishScanRequest:(id)request results:(id)results error:(id)error timeElapsed:(double)elapsed {
    BWRHStoreNetworkCollection(results);
    %orig(manager, request, results, error, elapsed);
}

- (void)scanManager:(id)manager willStartScanRequest:(id)request {
    if (BWRHBool(@"enabled", YES) && BWRHBool(@"unfilteredScan", NO) && request) {
        SEL applySel = NSSelectorFromString(@"setApplyRssiThresholdFilter:");
        SEL thresholdSel = NSSelectorFromString(@"setRssiThreshold:");
        SEL bssSel = NSSelectorFromString(@"setIncludeBSSList:");
        if ([request respondsToSelector:applySel]) ((void (*)(id, SEL, BOOL))objc_msgSend)(request, applySel, NO);
        if ([request respondsToSelector:thresholdSel]) ((void (*)(id, SEL, long long))objc_msgSend)(request, thresholdSel, -170);
        if ([request respondsToSelector:bssSel]) ((void (*)(id, SEL, BOOL))objc_msgSend)(request, bssSel, YES);
    }
    %orig(manager, request);
}

- (BOOL)scanManagerShouldSupportUnfilteredScanning:(id)manager {
    if (BWRHBool(@"enabled", YES) && BWRHBool(@"unfilteredScan", NO)) return YES;
    return %orig(manager);
}

%end

%hook WFNetworkSettingsViewController

- (void)viewDidLoad {
    %orig;
    ((void (*)(id, SEL))objc_msgSend)((id)self, NSSelectorFromString(@"bwrh_updateDetailsButton"));
}


%new
- (void)bwrh_updateDetailsButton {
    UIViewController *controller = (UIViewController *)self;
    UIBarButtonItem *item = objc_getAssociatedObject((id)self, BWRHDetailsButtonKey);
    BOOL shouldShow = BWRHShouldShowCurrentDetails((id)self);

    if (!shouldShow) {
        if (item) {
            NSMutableArray *items = [controller.navigationItem.rightBarButtonItems mutableCopy] ?: [NSMutableArray array];
            [items removeObject:item];
            controller.navigationItem.rightBarButtonItems = items;
        }
        return;
    }

    if (!item) {
        UIImage *image = nil;
        if (@available(iOS 13.0, *)) image = [UIImage systemImageNamed:@"wifi.circle"];
        item = image
            ? [[UIBarButtonItem alloc] initWithImage:image style:UIBarButtonItemStylePlain target:self action:@selector(bwrh_openConnectedDetails)]
            : [[UIBarButtonItem alloc] initWithTitle:@"BW" style:UIBarButtonItemStylePlain target:self action:@selector(bwrh_openConnectedDetails)];
        item.accessibilityLabel = @"BetterWiFi RH";
        objc_setAssociatedObject((id)self, BWRHDetailsButtonKey, item, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    NSArray *existing = controller.navigationItem.rightBarButtonItems ?: @[];
    if (![existing containsObject:item]) {
        controller.navigationItem.rightBarButtonItems = [existing arrayByAddingObject:item];
    }
}

%new
- (void)bwrh_openConnectedDetails {
    if (!BWRHShouldShowCurrentDetails((id)self)) return;
    UIViewController *source = (UIViewController *)self;
    UIViewController *airport = BWRHFindAirportController(source);
    BWRHConnectedDetailsController *details = [[BWRHConnectedDetailsController alloc] initWithSourceController:source airportController:airport];
    [source.navigationController pushViewController:details animated:YES];
}

%end

%ctor {
    @autoreleasepool {
        if (@available(iOS 16.0, *)) {
            CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), NULL, BWRHPreferencesChanged, (__bridge CFStringRef)BWRHPrefsChangedNotification, NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
            %init;
        }
    }
}
