from pathlib import Path
import plistlib

root=Path("BetterWiFi-RH")
controller=root/"prefs/BWRHRootListController.m"
s=controller.read_text()

# Credits spacing: give Dynamic Type and the subtitle more breathing room.
s=s.replace(
    'UIView *footer = [[UIView alloc] initWithFrame:CGRectMake(0, 0, CGRectGetWidth(table.bounds), 242.0)];',
    'UIView *footer = [[UIView alloc] initWithFrame:CGRectMake(0, 0, CGRectGetWidth(table.bounds), 254.0)];',
    1
)
s=s.replace(
    '[creditsLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:2.0],',
    '[creditsLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:8.0],',
    1
)
s=s.replace(
    '[buttons.topAnchor constraintEqualToAnchor:creditsLabel.bottomAnchor constant:12.0],',
    '[buttons.topAnchor constraintEqualToAnchor:creditsLabel.bottomAnchor constant:14.0],',
    1
)

# Make both labels explicitly single-line and shrink safely rather than visually colliding.
s=s.replace(
    'nameLabel.adjustsFontForContentSizeCategory = YES;',
    'nameLabel.adjustsFontForContentSizeCategory = YES;\n    nameLabel.numberOfLines = 1;\n    nameLabel.adjustsFontSizeToFitWidth = YES;\n    nameLabel.minimumScaleFactor = 0.85;',
    1
)
s=s.replace(
    'creditsLabel.adjustsFontForContentSizeCategory = YES;',
    'creditsLabel.adjustsFontForContentSizeCategory = YES;\n    creditsLabel.numberOfLines = 1;',
    1
)

# Force the entire Advanced Filters section to English by preference key when
# English is manually selected. This avoids relying on reverse-localizing text
# that Shuffle/Preferences may already have transformed.
anchor='''- (void)bwrh_applyLanguageToSpecifiers {
    NSInteger mode = BWRHPrefsLanguageMode();
    if (mode == 0) return;

    for (PSSpecifier *specifier in _specifiers) {
        NSString *name = specifier.name;
        if (name.length) specifier.name = BWRHPreferenceStringForMode(name, mode);

        NSString *footer = [specifier propertyForKey:@"footerText"];
        if ([footer isKindOfClass:[NSString class]] && footer.length) {
            [specifier setProperty:BWRHPreferenceStringForMode(footer, mode) forKey:@"footerText"];
        }

        NSArray *titles = [specifier propertyForKey:@"validTitles"];
        if ([titles isKindOfClass:[NSArray class]]) {
            [specifier setProperty:BWRHPreferenceTitlesForMode(titles, mode) forKey:@"validTitles"];
        }
    }
}
'''
replacement='''- (void)bwrh_applyLanguageToSpecifiers {
    NSInteger mode = BWRHPrefsLanguageMode();
    if (mode == 0) return;

    BOOL insideAdvancedFilters = NO;
    for (PSSpecifier *specifier in _specifiers) {
        NSString *name = specifier.name;
        if (name.length) specifier.name = BWRHPreferenceStringForMode(name, mode);

        NSString *footer = [specifier propertyForKey:@"footerText"];
        if ([footer isKindOfClass:[NSString class]] && footer.length) {
            [specifier setProperty:BWRHPreferenceStringForMode(footer, mode) forKey:@"footerText"];
        }

        NSArray *titles = [specifier propertyForKey:@"validTitles"];
        if ([titles isKindOfClass:[NSArray class]]) {
            [specifier setProperty:BWRHPreferenceTitlesForMode(titles, mode) forKey:@"validTitles"];
        }

        if (mode == 2) {
            NSString *key = [specifier propertyForKey:@"key"];
            NSString *currentName = specifier.name ?: @"";

            if ([currentName caseInsensitiveCompare:@"Advanced filters"] == NSOrderedSame ||
                [currentName caseInsensitiveCompare:@"Filtros avanzados"] == NSOrderedSame) {
                insideAdvancedFilters = YES;
                specifier.name = @"Advanced filters";
                [specifier setProperty:@"Filters are applied only to the Wi-Fi list. The channel analyzer keeps the complete scan snapshot."
                                forKey:@"footerText"];
                continue;
            }

            if ([currentName caseInsensitiveCompare:@"BetterWiFi classic"] == NSOrderedSame ||
                [currentName caseInsensitiveCompare:@"BetterWiFi clásico"] == NSOrderedSame) {
                insideAdvancedFilters = NO;
            }

            if (insideAdvancedFilters) {
                if ([key isEqualToString:@"bandFilter"]) {
                    specifier.name = @"Band filter";
                    [specifier setProperty:@[@"All", @"2.4", @"5"] forKey:@"validTitles"];
                } else if ([key isEqualToString:@"networkTypeFilter"]) {
                    specifier.name = @"Network type";
                    [specifier setProperty:@[@"All", @"Known", @"New"] forKey:@"validTitles"];
                } else if ([key isEqualToString:@"minimumRSSIEnabled"]) {
                    specifier.name = @"Minimum signal filter";
                } else if ([key isEqualToString:@"minimumRSSI"]) {
                    specifier.name = @"Minimum dBm";
                    [specifier setProperty:@[@"-95", @"-90", @"-85", @"-80", @"-75", @"-70"] forKey:@"validTitles"];
                } else if ([key isEqualToString:@"sortMode"]) {
                    specifier.name = @"Sort networks";
                    [specifier setProperty:@[@"iOS", @"Signal", @"Name", @"Ch."] forKey:@"validTitles"];
                }
            }
        }
    }
}
'''
if anchor not in s:
    raise SystemExit("language method anchor not found")
s=s.replace(anchor,replacement,1)
controller.write_text(s)

# Version bump.
p=root/"control"
t=p.read_text().replace("Version: 0.3.11","Version: 0.3.12")
if "Version: 0.3.12" not in t:
    raise SystemExit("control version bump failed")
p.write_text(t)

p=root/"prefs/Resources/Info.plist"
info=plistlib.loads(p.read_bytes())
info["CFBundleShortVersionString"]="0.3.12"
info["CFBundleVersion"]="15"
p.write_bytes(plistlib.dumps(info,fmt=plistlib.FMT_XML,sort_keys=False))

print("BetterWiFi RH 0.3.12 credits spacing + English advanced filters patch applied")
