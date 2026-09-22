from pathlib import Path
p=Path('BetterWiFi-RH/Tweak.xm')
s=p.read_text()

old='''static NSNumber *BWRHChannelNumber(id network) {
    id channel = BWRHMsgObject(network, @"channel");
    if ([channel isKindOfClass:[NSNumber class]]) return channel;

    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"channel")) {
        unsigned long long value = BWRHMsgUnsignedLongLong(cwfChannel, @"channel", 0);
        if (value > 0 && value < 1000) return @(value);
    }
    return nil;
}
'''
new='''static NSNumber *BWRHChannelNumber(id network) {
    id channel = BWRHMsgObject(network, @"channel");
    if ([channel isKindOfClass:[NSNumber class]]) return channel;
    if (channel && BWRHResponds(channel, @"channel")) {
        unsigned long long value = BWRHMsgUnsignedLongLong(channel, @"channel", 0);
        if (value > 0 && value < 1000) return @(value);
    }

    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"channel")) {
        unsigned long long value = BWRHMsgUnsignedLongLong(cwfChannel, @"channel", 0);
        if (value > 0 && value < 1000) return @(value);
    }
    return nil;
}

static id BWRHChannelObject(id network) {
    id channel = BWRHMsgObject(network, @"channel");
    if (channel && ![channel isKindOfClass:[NSNumber class]] && BWRHResponds(channel, @"channel")) return channel;
    return BWRHCWFChannel(network);
}

static long long BWRHNetworkRSSI(id network, long long fallback) {
    long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);
    if (rssi == LLONG_MIN) rssi = BWRHMsgLongLong(network, @"RSSI", LLONG_MIN);
    if (rssi == LLONG_MIN) {
        id scanResult = BWRHMsgObject(network, @"scanResult");
        rssi = BWRHMsgLongLong(scanResult, @"RSSI", LLONG_MIN);
        if (rssi == LLONG_MIN) rssi = BWRHMsgLongLong(scanResult, @"rssi", LLONG_MIN);
    }
    return rssi == LLONG_MIN ? fallback : rssi;
}
'''
if old not in s: raise SystemExit('channel block missing')
s=s.replace(old,new)

old='''static NSString *BWRHBandDescription(id network) {
    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel) {
        if (BWRHMsgBool(cwfChannel, @"is2GHz", NO)) return @"2.4 GHz";
        if (BWRHMsgBool(cwfChannel, @"is5GHz", NO)) return @"5 GHz";
        if (BWRHMsgBool(cwfChannel, @"is6GHz", NO)) return @"6 GHz";
    }
'''
new='''static NSString *BWRHBandDescription(id network) {
    id cwfChannel = BWRHChannelObject(network);
    if (cwfChannel) {
        if (BWRHMsgBool(cwfChannel, @"is2GHz", NO)) return @"2.4 GHz";
        if (BWRHMsgBool(cwfChannel, @"is5GHz", NO)) return @"5 GHz";
        if (BWRHMsgBool(cwfChannel, @"is6GHz", NO)) return @"6 GHz";
        int band = BWRHMsgInt(cwfChannel, @"band", -1);
        if (band == 1) return @"2.4 GHz";
        if (band == 2) return @"5 GHz";
        if (band == 3) return @"6 GHz";
    }
'''
if old not in s: raise SystemExit('band block missing')
s=s.replace(old,new)

old='''@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic, strong) UILabel *valueLabel;
'''
new='''@property (nonatomic) NSUInteger monitorGeneration;
@property (nonatomic, strong) UILabel *valueLabel;
'''
if old not in s: raise SystemExit('timer property missing')
s=s.replace(old,new)

old='''- (void)startMonitoring {
    [self.timer invalidate];
    self.timer = nil;
    [self sampleSignal];
    NSTimeInterval interval = BWRHBool(@"efficientMode", YES) ? 3.0 : 1.5;
    __weak typeof(self) weakSelf = self;
    self.timer = [NSTimer scheduledTimerWithTimeInterval:interval repeats:YES block:^(NSTimer *timer) {
        [weakSelf sampleSignal];
    }];
}

- (void)stopMonitoring {
    [self.timer invalidate];
    self.timer = nil;
}
'''
new='''- (void)scheduleNextSampleForGeneration:(NSUInteger)generation {
    NSTimeInterval interval = BWRHBool(@"efficientMode", YES) ? 3.0 : 1.5;
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(interval * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        __strong typeof(weakSelf) self = weakSelf;
        if (!self || self.monitorGeneration != generation || !self.view.window) return;
        [self sampleSignal];
        [self scheduleNextSampleForGeneration:generation];
    });
}

- (void)startMonitoring {
    self.monitorGeneration += 1;
    NSUInteger generation = self.monitorGeneration;
    [self sampleSignal];
    [self scheduleNextSampleForGeneration:generation];
}

- (void)stopMonitoring {
    self.monitorGeneration += 1;
}
'''
if old not in s: raise SystemExit('monitor block missing')
s=s.replace(old,new)

s=s.replace('long long rssi = BWRHMsgLongLong(network, @"rssi", -999);','long long rssi = BWRHNetworkRSSI(network, -999);')
s=s.replace('long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);','long long rssi = BWRHNetworkRSSI(network, LLONG_MIN);')
s=s.replace('static long long BWRHNetworkRSSI(id network, long long fallback) {\n    long long rssi = BWRHNetworkRSSI(network, LLONG_MIN);','static long long BWRHNetworkRSSI(id network, long long fallback) {\n    long long rssi = BWRHMsgLongLong(network, @"rssi", LLONG_MIN);')

# Improve channel width for direct CWFChannel objects too.
old='''    id cwfChannel = BWRHCWFChannel(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"width")) {
'''
new='''    id cwfChannel = BWRHChannelObject(network);
    if (cwfChannel && BWRHResponds(cwfChannel, @"width")) {
'''
if old not in s: raise SystemExit('width channel block missing')
s=s.replace(old,new,1)

# Add analyzer debug footer counts so device testing is actionable.
old='''    if (section == 1 && self.rows5.count == 0) return BWRHT(@"No se detectaron redes de 5 GHz en el último escaneo.", @"No 5 GHz networks were detected in the last scan.");
    return nil;
}
'''
new='''    if (section == 1 && self.rows5.count == 0) {
        NSInteger total = self.networks.count;
        return [NSString stringWithFormat:BWRHT(@"No se detectaron redes de 5 GHz. Registros recibidos: %ld. Pulsa actualizar para forzar un nuevo escaneo.", @"No 5 GHz networks were detected. Records received: %ld. Tap refresh to force a new scan."), (long)total];
    }
    if (section == 1) {
        return [NSString stringWithFormat:BWRHT(@"%ld redes/registros analizados.", @"%ld networks/records analyzed."), (long)self.networks.count];
    }
    return nil;
}
'''
if old not in s: raise SystemExit('footer block missing')
s=s.replace(old,new)

p.write_text(s)

cp=Path('BetterWiFi-RH/control')
t=cp.read_text().replace('Version: 0.3.2','Version: 0.3.3')
cp.write_text(t)

import plistlib
ip=Path('BetterWiFi-RH/prefs/Resources/Info.plist')
info=plistlib.loads(ip.read_bytes())
info['CFBundleShortVersionString']='0.3.3'
info['CFBundleVersion']='6'
ip.write_bytes(plistlib.dumps(info,fmt=plistlib.FMT_XML,sort_keys=False))
