#import "BWRHRootListController.h"
#import <Preferences/PSSpecifier.h>
#import <UIKit/UIKit.h>

static NSString * const BWRHPrefsDomain = @"com.betterwifirh.preferences";

static NSInteger BWRHPrefsLanguageMode(void) {
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:BWRHPrefsDomain];
    id value = [defaults objectForKey:@"language"];
    NSInteger mode = value ? [value integerValue] : 0;
    return (mode >= 0 && mode <= 2) ? mode : 0;
}

static NSDictionary *BWRHSpanishPreferenceStrings(void) {
    static NSDictionary *strings;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSBundle *bundle = [NSBundle bundleForClass:[BWRHRootListController class]];
        NSString *path = [bundle pathForResource:@"Root" ofType:@"strings" inDirectory:@"es.lproj"];
        NSDictionary *loaded = path ? [NSDictionary dictionaryWithContentsOfFile:path] : nil;
        strings = loaded ?: @{};
    });
    return strings;
}

static NSString *BWRHPreferenceStringForMode(NSString *value, NSInteger mode) {
    if (!value.length || mode == 0) return value;

    NSDictionary *spanish = BWRHSpanishPreferenceStrings();
    if (mode == 1) {
        NSString *translated = spanish[value];
        if (translated.length) return translated;
        // The system may already have localized the string to Spanish.
        if ([[spanish allValues] containsObject:value]) return value;
        return value;
    }

    // English mode: Root.plist uses English keys, so reverse a Spanish value
    // if the system localized the specifier before we got it.
    for (NSString *english in spanish) {
        NSString *translated = spanish[english];
        if ([translated isEqualToString:value]) return english;
    }
    return value;
}

static NSArray *BWRHPreferenceTitlesForMode(NSArray *titles, NSInteger mode) {
    if (![titles isKindOfClass:[NSArray class]] || mode == 0) return titles;
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:titles.count];
    for (id title in titles) {
        if ([title isKindOfClass:[NSString class]]) {
            [result addObject:BWRHPreferenceStringForMode(title, mode)];
        } else {
            [result addObject:title];
        }
    }
    return result;
}

@implementation BWRHRootListController

- (NSString *)bwrh_creditsTextSpanish:(NSString *)spanish english:(NSString *)english {
    NSInteger mode = BWRHPrefsLanguageMode();
    if (mode == 1) return spanish;
    if (mode == 2) return english;
    NSString *language = [NSLocale preferredLanguages].firstObject ?: @"en";
    return [[language lowercaseString] hasPrefix:@"es"] ? spanish : english;
}

- (UIImage *)bwrh_creditsAvatarImage {
    NSBundle *bundle = [NSBundle bundleForClass:[self class]];
    NSString *path = [bundle pathForResource:@"CreditsAvatar" ofType:@"jpg"];
    UIImage *source = path.length ? [UIImage imageWithContentsOfFile:path] : nil;
    if (!source) return [UIImage systemImageNamed:@"person.crop.circle.fill"];

    CGSize size = CGSizeMake(64.0, 64.0);
    UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithSize:size];
    return [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        CGRect bounds = (CGRect){CGPointZero, size};
        [[UIBezierPath bezierPathWithOvalInRect:bounds] addClip];
        CGFloat scale = MAX(size.width / source.size.width, size.height / source.size.height);
        CGSize drawSize = CGSizeMake(source.size.width * scale, source.size.height * scale);
        CGRect drawRect = CGRectMake((size.width - drawSize.width) * 0.5,
                                     (size.height - drawSize.height) * 0.5,
                                     drawSize.width,
                                     drawSize.height);
        [source drawInRect:drawRect];
    }];
}

- (UIButton *)bwrh_creditsButtonWithTitle:(NSString *)title symbol:(NSString *)symbol action:(SEL)action {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    UIButtonConfiguration *configuration = [UIButtonConfiguration tintedButtonConfiguration];
    configuration.title = title;
    configuration.image = symbol.length ? [UIImage systemImageNamed:symbol] : nil;
    configuration.imagePadding = 8.0;
    configuration.cornerStyle = UIButtonConfigurationCornerStyleMedium;
    button.configuration = configuration;
    [button addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    [button.heightAnchor constraintEqualToConstant:44.0].active = YES;
    return button;
}

- (void)bwrh_openRepository {
    NSURL *url = [NSURL URLWithString:@"https://github.com/Gokuencinar/GokuEnREPO"];
    if (url) [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];
}

- (void)bwrh_buyCoffee {
    NSURL *url = [NSURL URLWithString:@"https://buymeacoffee.com/gokuen"];
    if (url) [UIApplication.sharedApplication openURL:url options:@{} completionHandler:nil];
}

- (void)bwrh_installCreditsFooter {
    UITableView *table = [self table];
    if (!table) return;

    UIView *footer = [[UIView alloc] initWithFrame:CGRectMake(0, 0, CGRectGetWidth(table.bounds), 254.0)];
    footer.autoresizingMask = UIViewAutoresizingFlexibleWidth;

    UIImageView *avatarView = [[UIImageView alloc] initWithImage:[self bwrh_creditsAvatarImage]];
    avatarView.translatesAutoresizingMaskIntoConstraints = NO;
    avatarView.contentMode = UIViewContentModeScaleAspectFill;
    avatarView.accessibilityLabel = @"Gokuencinar · GokuEn";

    UILabel *nameLabel = [[UILabel alloc] init];
    nameLabel.translatesAutoresizingMaskIntoConstraints = NO;
    nameLabel.text = @"Gokuencinar · GokuEn";
    nameLabel.font = [UIFont systemFontOfSize:17.0 weight:UIFontWeightSemibold];
    nameLabel.textAlignment = NSTextAlignmentCenter;
    nameLabel.adjustsFontForContentSizeCategory = YES;
    nameLabel.numberOfLines = 1;
    nameLabel.adjustsFontSizeToFitWidth = YES;
    nameLabel.minimumScaleFactor = 0.85;

    UILabel *creditsLabel = [[UILabel alloc] init];
    creditsLabel.translatesAutoresizingMaskIntoConstraints = NO;
    creditsLabel.text = [self bwrh_creditsTextSpanish:@"Créditos de BetterWiFi RH" english:@"BetterWiFi RH credits"];
    creditsLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    creditsLabel.textColor = UIColor.secondaryLabelColor;
    creditsLabel.textAlignment = NSTextAlignmentCenter;
    creditsLabel.adjustsFontForContentSizeCategory = YES;
    creditsLabel.numberOfLines = 1;

    UIButton *repoButton = [self bwrh_creditsButtonWithTitle:
        [self bwrh_creditsTextSpanish:@"Visitar GokuEnREPO" english:@"Visit GokuEnREPO"]
        symbol:@"link"
        action:@selector(bwrh_openRepository)];

    UIButton *coffeeButton = [self bwrh_creditsButtonWithTitle:@"Buy Me a Coffee"
        symbol:@"cup.and.saucer.fill"
        action:@selector(bwrh_buyCoffee)];

    UIStackView *buttons = [[UIStackView alloc] initWithArrangedSubviews:@[repoButton, coffeeButton]];
    buttons.translatesAutoresizingMaskIntoConstraints = NO;
    buttons.axis = UILayoutConstraintAxisVertical;
    buttons.spacing = 8.0;
    buttons.alignment = UIStackViewAlignmentFill;
    buttons.distribution = UIStackViewDistributionFillEqually;

    [footer addSubview:avatarView];
    [footer addSubview:nameLabel];
    [footer addSubview:creditsLabel];
    [footer addSubview:buttons];

    [NSLayoutConstraint activateConstraints:@[
        [avatarView.topAnchor constraintEqualToAnchor:footer.topAnchor constant:18.0],
        [avatarView.centerXAnchor constraintEqualToAnchor:footer.centerXAnchor],
        [avatarView.widthAnchor constraintEqualToConstant:64.0],
        [avatarView.heightAnchor constraintEqualToConstant:64.0],

        [nameLabel.topAnchor constraintEqualToAnchor:avatarView.bottomAnchor constant:8.0],
        [nameLabel.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:20.0],
        [nameLabel.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-20.0],

        [creditsLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:8.0],
        [creditsLabel.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:20.0],
        [creditsLabel.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-20.0],

        [buttons.topAnchor constraintEqualToAnchor:creditsLabel.bottomAnchor constant:14.0],
        [buttons.leadingAnchor constraintEqualToAnchor:footer.leadingAnchor constant:32.0],
        [buttons.trailingAnchor constraintEqualToAnchor:footer.trailingAnchor constant:-32.0],
        [buttons.bottomAnchor constraintLessThanOrEqualToAnchor:footer.bottomAnchor constant:-14.0]
    ]];

    table.tableFooterView = footer;
}


- (void)bwrh_applyLanguageToSpecifiers {
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

- (NSArray *)specifiers {
    if (!_specifiers) {
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        _specifiers = [self loadSpecifiersFromPlistName:@"Root" target:self bundle:bundle];
        if (!_specifiers) _specifiers = [NSMutableArray array];
        [self bwrh_applyLanguageToSpecifiers];
    }
    return _specifiers;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"BetterWiFi RH";
    [self bwrh_installCreditsFooter];
}

- (void)setPreferenceValue:(id)value specifier:(PSSpecifier *)specifier {
    [super setPreferenceValue:value specifier:specifier];

    NSString *key = [specifier propertyForKey:@"key"];
    if ([key isEqualToString:@"language"]) {
        NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:BWRHPrefsDomain];
        [defaults setObject:value forKey:@"language"];
        [defaults synchronize];

        dispatch_async(dispatch_get_main_queue(), ^{
            self->_specifiers = nil;
            [self reloadSpecifiers];
            self.title = @"BetterWiFi RH";
            [self bwrh_installCreditsFooter];
        });
    }
}

@end
