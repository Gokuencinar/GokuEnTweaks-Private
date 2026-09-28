from pathlib import Path
p=Path('BetterWiFi-RH/Tweak.xm')
s=p.read_text()

anchor='''static void BWRHRequestFreshScan(id airportController) {
    id listController = BWRHNetworkListControllerForAirport(airportController);
    if (BWRHResponds(listController, @"startScanning")) ((void (*)(id, SEL))objc_msgSend)(listController, NSSelectorFromString(@"startScanning"));
    if (BWRHResponds(airportController, @"refresh")) ((void (*)(id, SEL))objc_msgSend)(airportController, NSSelectorFromString(@"refresh"));
}

#pragma mark - Signal monitor
'''
insert='''static void BWRHRequestFreshScan(id airportController) {
    id listController = BWRHNetworkListControllerForAirport(airportController);
    if (BWRHResponds(listController, @"startScanning")) ((void (*)(id, SEL))objc_msgSend)(listController, NSSelectorFromString(@"startScanning"));
    if (BWRHResponds(airportController, @"refresh")) ((void (*)(id, SEL))objc_msgSend)(airportController, NSSelectorFromString(@"refresh"));
}

static void BWRHAsyncCurrentNetwork(id airportController, void (^completion)(id network)) {
    id wfInterface = BWRHWFInterfaceForAirport(airportController);
    if (wfInterface && BWRHResponds(wfInterface, @"asyncCurrentNetwork:")) {
        void (^reply)(id) = ^(id network) {
            if (completion) completion(network);
        };
        ((void (*)(id, SEL, id))objc_msgSend)(wfInterface, NSSelectorFromString(@"asyncCurrentNetwork:"), reply);
        return;
    }
    if (completion) completion(BWRHMsgObject(wfInterface, @"currentNetwork"));
}

static void BWRHPerformRawCoreWiFiScan(id airportController, void (^completion)(NSArray *results, NSError *error)) {
    id cInterface = BWRHCWFInterfaceForAirport(airportController);
    Class paramsClass = NSClassFromString(@"CWFScanParameters");
    SEL scanSel = NSSelectorFromString(@"performScanWithParameters:reply:");
    if (!cInterface || !paramsClass || ![cInterface respondsToSelector:scanSel]) {
        if (completion) completion(nil, nil);
        return;
    }

    id params = [[paramsClass alloc] init];
    if ([params respondsToSelector:NSSelectorFromString(@"setCacheOnly:")])
        ((void (*)(id, SEL, BOOL))objc_msgSend)(params, NSSelectorFromString(@"setCacheOnly:"), NO);
    if ([params respondsToSelector:NSSelectorFromString(@"setMergeScanResults:")])
        ((void (*)(id, SEL, BOOL))objc_msgSend)(params, NSSelectorFromString(@"setMergeScanResults:"), YES);
    if ([params respondsToSelector:NSSelectorFromString(@"setIncludeHiddenNetworks:")])
        ((void (*)(id, SEL, BOOL))objc_msgSend)(params, NSSelectorFromString(@"setIncludeHiddenNetworks:"), YES);
    if ([params respondsToSelector:NSSelectorFromString(@"setMinimumRSSI:")])
        ((void (*)(id, SEL, long long))objc_msgSend)(params, NSSelectorFromString(@"setMinimumRSSI:"), -100);
    if ([params respondsToSelector:NSSelectorFromString(@"setMaximumAge:")])
        ((void (*)(id, SEL, unsigned long long))objc_msgSend)(params, NSSelectorFromString(@"setMaximumAge:"), 0);
    if ([params respondsToSelector:NSSelectorFromString(@"setMaximumCacheAge:")])
        ((void (*)(id, SEL, unsigned long long))objc_msgSend)(params, NSSelectorFromString(@"setMaximumCacheAge:"), 0);
    if ([params respondsToSelector:NSSelectorFromString(@"setNumberOfScans:")])
        ((void (*)(id, SEL, unsigned long long))objc_msgSend)(params, NSSelectorFromString(@"setNumberOfScans:"), 1);

    void (^reply)(id, id) = ^(id first, id second) {
        NSArray *results = nil;
        NSError *error = nil;
        if ([first isKindOfClass:[NSArray class]]) results = first;
        if ([second isKindOfClass:[NSArray class]]) results = second;
        if ([first isKindOfClass:[NSError class]]) error = first;
        if ([second isKindOfClass:[NSError class]]) error = second;
        if (completion) completion(results, error);
    };
    ((void (*)(id, SEL, id, id))objc_msgSend)(cInterface, scanSel, params, reply);
}

#pragma mark - Signal monitor
'''
if anchor not in s: raise SystemExit('helper anchor missing')
s=s.replace(anchor,insert)

old='''- (void)sampleSignal {
    id fallback = [self currentNetwork];
    long long rssi = BWRHLiveRSSIForAirport(self.airportController, fallback);
    if (rssi != LLONG_MIN) {
        [self.samples addObject:@(rssi)];
        while (self.samples.count > 60) [self.samples removeObjectAtIndex:0];
        self.valueLabel.text = [NSString stringWithFormat:@"%lld dBm", rssi];
        self.qualityLabel.text = BWRHSignalQuality(rssi);
        self.graphView.samples = self.samples;
    } else {
        self.valueLabel.text = @"— dBm";
        self.qualityLabel.text = BWRHT(@"RSSI no disponible", @"RSSI unavailable");
    }
    id liveNetwork = BWRHLiveScanResultForAirport(self.airportController) ?: fallback;
    NSNumber *channel = BWRHChannelNumber(liveNetwork);
    NSString *band = BWRHBandDescription(liveNetwork);
    NSString *bssid = BWRHBSSID(liveNetwork);
    NSMutableArray *meta = [NSMutableArray array];
    if (band.length) [meta addObject:band];
    if (channel) [meta addObject:[NSString stringWithFormat:@"%@ %@", BWRHT(@"Canal", @"Channel"), channel]];
    if (bssid.length) [meta addObject:bssid];
    self.metaLabel.text = [meta componentsJoinedByString:@" · "];
}
'''
new='''- (void)applyLiveNetworkSample:(id)network generation:(NSUInteger)generation {
    if (self.monitorGeneration != generation || !self.view.window) return;
    id fallback = [self currentNetwork];
    long long rssi = BWRHNetworkRSSI(network, LLONG_MIN);
    if (rssi == LLONG_MIN) rssi = BWRHLiveRSSIForAirport(self.airportController, fallback);
    if (rssi != LLONG_MIN) {
        [self.samples addObject:@(rssi)];
        while (self.samples.count > 60) [self.samples removeObjectAtIndex:0];
        self.valueLabel.text = [NSString stringWithFormat:@"%lld dBm", rssi];
        self.qualityLabel.text = BWRHSignalQuality(rssi);
        self.graphView.samples = [self.samples copy];
    } else {
        self.valueLabel.text = @"— dBm";
        self.qualityLabel.text = BWRHT(@"RSSI no disponible", @"RSSI unavailable");
    }

    id liveNetwork = network ?: BWRHLiveScanResultForAirport(self.airportController) ?: fallback;
    NSNumber *channel = BWRHChannelNumber(liveNetwork);
    NSString *band = BWRHBandDescription(liveNetwork);
    NSString *bssid = BWRHBSSID(liveNetwork);
    NSMutableArray *meta = [NSMutableArray array];
    if (band.length) [meta addObject:band];
    if (channel) [meta addObject:[NSString stringWithFormat:@"%@ %@", BWRHT(@"Canal", @"Channel"), channel]];
    if (bssid.length) [meta addObject:bssid];
    self.metaLabel.text = [meta componentsJoinedByString:@" · "];
}

- (void)sampleSignal {
    NSUInteger generation = self.monitorGeneration;
    __weak typeof(self) weakSelf = self;
    BWRHAsyncCurrentNetwork(self.airportController, ^(id network) {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self) return;
            [self applyLiveNetworkSample:network generation:generation];
        });
    });
}
'''
if old not in s: raise SystemExit('sampleSignal block missing')
s=s.replace(old,new)

old='''- (void)refreshScan {
    BWRHRequestFreshScan(self.airportController);
    self.navigationItem.rightBarButtonItem.enabled = NO;
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.8 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        weakSelf.networks = BWRHLatestScanNetworks(weakSelf.airportController);
        [weakSelf rebuildRows];
        [weakSelf.tableView reloadData];
        weakSelf.navigationItem.rightBarButtonItem.enabled = YES;
    });
}
'''
new='''- (void)refreshScan {
    self.navigationItem.rightBarButtonItem.enabled = NO;
    __weak typeof(self) weakSelf = self;
    BWRHPerformRawCoreWiFiScan(self.airportController, ^(NSArray *results, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self) return;
            if (results.count) {
                self.networks = results;
                BWRHStoreNetworkSnapshot(results);
                [self rebuildRows];
                [self.tableView reloadData];
                self.navigationItem.rightBarButtonItem.enabled = YES;
                return;
            }

            BWRHRequestFreshScan(self.airportController);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.8 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                self.networks = BWRHLatestScanNetworks(self.airportController);
                [self rebuildRows];
                [self.tableView reloadData];
                self.navigationItem.rightBarButtonItem.enabled = YES;
            });
        });
    });
}
'''
if old not in s: raise SystemExit('refreshScan block missing')
s=s.replace(old,new)

p.write_text(s)

# 0.3.4
cp=Path('BetterWiFi-RH/control')
t=cp.read_text().replace('Version: 0.3.3','Version: 0.3.4')
cp.write_text(t)

import plistlib
ip=Path('BetterWiFi-RH/prefs/Resources/Info.plist')
info=plistlib.loads(ip.read_bytes())
info['CFBundleShortVersionString']='0.3.4'
info['CFBundleVersion']='7'
ip.write_bytes(plistlib.dumps(info,fmt=plistlib.FMT_XML,sort_keys=False))
