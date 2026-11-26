#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "desktop_wrapper.h"
#include "panel_ui.h"
#include "panel_presentation.h"

#ifndef HDSLAVE_OBJC_VIEW_CLASS
#define HDSLAVE_OBJC_VIEW_CLASS HdslavePanelView
#endif

static uint32_t modsFromEvent (NSEvent* event)
{
    uint32_t m = 0;
    const NSEventModifierFlags f = [event modifierFlags];
    if (f & NSEventModifierFlagShift) m |= hdslave_desktop::kModShift;
    if (f & NSEventModifierFlagOption) m |= hdslave_desktop::kModAlt;
    if (f & NSEventModifierFlagCommand) m |= hdslave_desktop::kModCmd;
    return m;
}

@interface HDSLAVE_OBJC_VIEW_CLASS : NSView
{
@public
    hdslave_desktop::DesktopPlugin* _plugin;
    NSTimer* _timer;
    NSTrackingArea* _trackingArea;
    hdslave_desktop::PanelPresentation _presented;
}
- (instancetype)initWithPlugin:(hdslave_desktop::DesktopPlugin*)plugin frame:(NSRect)frame;
- (void)startTimer;
- (void)stopTimer;
- (void)syncSize;
- (void)pumpFrame;
@end

@implementation HDSLAVE_OBJC_VIEW_CLASS

- (instancetype)initWithPlugin:(hdslave_desktop::DesktopPlugin*)plugin frame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self)
    {
        _plugin = plugin;
        _timer = nil;
        _trackingArea = nil;
        self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [self setAllowedTouchTypes:NSTouchTypeMaskIndirect];
    }
    return self;
}

- (void)dealloc
{
    [self stopTimer];
    if (_trackingArea)
    {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
        _trackingArea = nil;
    }
    [super dealloc];
}

- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return YES; }
- (BOOL)acceptsFirstResponder
{
    // Let the host keep both halves of a typing-keyboard note across panel clicks.
    // Keyboard navigation can still focus the editor for arrow keys and Escape.
    NSEvent* event = NSApp.currentEvent;
    return event.window != self.window ||
           (event.type != NSEventTypeLeftMouseDown && event.type != NSEventTypeRightMouseDown &&
            event.type != NSEventTypeOtherMouseDown);
}
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }

- (int)backingScaleInt
{
    CGFloat sf = 2.0;
    if (self.window)
        sf = self.window.backingScaleFactor;
    else if ([NSScreen mainScreen])
        sf = [[NSScreen mainScreen] backingScaleFactor];
    return std::clamp (int (std::round (sf)), 1, 3);
}

- (void)syncSize
{
    if (!_plugin || !_plugin->ui) return;
    const int scale = [self backingScaleInt];
    const int w = std::max (320, int (std::round (self.bounds.size.width)));
    const int h = std::max (240, int (std::round (self.bounds.size.height)));
    _plugin->ui->setSize (w, h, scale);
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self syncSize];
    if (self.window)
    {
        if (_plugin && _plugin->ui) _plugin->ui->invalidateAll();
        [self pumpFrame];
    }
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self syncSize];
    if (_plugin && _plugin->ui) _plugin->ui->invalidateAll();
    [self pumpFrame];
}

- (void)resizeWithOldSuperviewSize:(NSSize)oldSize
{
    if (!_plugin || !_plugin->surface)
    {
        [super resizeWithOldSuperviewSize:oldSize];
        return;
    }
    // A CLAP host may update its container before or after calling set_size.
    // Following its absolute bounds avoids applying the resize delta twice.
    if (NSEqualSizes (oldSize, self.frame.size))
        [self setFrameSize:self.superview.bounds.size];
}

- (void)setFrameSize:(NSSize)newSize
{
    if (_plugin && _plugin->surface && self.superview)
    {
        const auto w = uint32_t (std::max (0L, std::lround (newSize.width)));
        const auto h = uint32_t (std::max (0L, std::lround (newSize.height)));
        if (w != _plugin->guiWidth || h != _plugin->guiHeight)
            _plugin->acceptGuiSize (w, h);
        newSize = NSMakeSize (_plugin->guiWidth, _plugin->guiHeight);
    }
    [super setFrameSize:newSize];
    [self syncSize];
    [self pumpFrame];
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if (_trackingArea)
    {
        [self removeTrackingArea:_trackingArea];
        [_trackingArea release];
        _trackingArea = nil;
    }
    NSTrackingAreaOptions opts = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
                               | NSTrackingActiveAlways | NSTrackingInVisibleRect;
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds options:opts owner:self userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)startTimer
{
    if (_timer) return;
    _timer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                              target:self
                                            selector:@selector(onTick:)
                                            userInfo:nil
                                             repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
}

- (void)stopTimer
{
    if (_timer)
    {
        [_timer invalidate];
        _timer = nil;
    }
}

- (void)pumpFrame
{
    if (!_plugin || !_plugin->ui) return;
    [self syncSize];
    auto& ui = *_plugin->ui;
    const auto dirty = ui.update (hdslave_desktop::nowMs());
    _presented.publish (ui.pixels(), ui.physWidth(), ui.physHeight(), ui.scale(), dirty,
        [self] (hdslave_desktop::Rect r) { [self setNeedsDisplayInRect:NSMakeRect (r.x, r.y, r.w, r.h)]; });
}

- (void)onTick:(NSTimer*)timer
{
    (void)timer;
    [self pumpFrame];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (!_plugin || !_plugin->ui) return;
    [self syncSize];
    if (_presented.width != _plugin->ui->physWidth() || _presented.height != _plugin->ui->physHeight() ||
        _presented.scale != _plugin->ui->scale())
        [self pumpFrame];
    const int pw = _presented.width, ph = _presented.height, sc = _presented.scale;
    if (!pw || !ph) return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    if (!ctx) return;
    const int x = std::clamp (int (std::floor (NSMinX (dirtyRect) * sc)), 0, pw);
    const int y = std::clamp (int (std::floor (NSMinY (dirtyRect) * sc)), 0, ph);
    const int w = std::clamp (int (std::ceil (NSMaxX (dirtyRect) * sc)), 0, pw) - x;
    const int h = std::clamp (int (std::ceil (NSMaxY (dirtyRect) * sc)), 0, ph) - y;
    if (w <= 0 || h <= 0) return;

    // Quartz may retain the image after drawRect returns. Give it an owned copy
    // of just this patch so a later animation tick or resize cannot change it.
    const size_t stride = size_t (w) * sizeof (uint32_t);
    CFMutableDataRef data = CFDataCreateMutable (nullptr, stride * h);
    if (!data) return;
    CFDataSetLength (data, stride * h);
    auto* dst = CFDataGetMutableBytePtr (data);
    for (int row = 0; row < h; ++row)
        std::memcpy (dst + row * stride, _presented.pixels.data() + size_t (y + row) * pw + x, stride);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData (data);
    CFRelease (data);
    if (!provider) return;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate (w, h, 8, 32, stride, cs,
        kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst, provider, nullptr, false, kCGRenderingIntentDefault);
    CGDataProviderRelease (provider);
    CGColorSpaceRelease (cs);
    if (!image) return;

    CGContextSaveGState (ctx);
    CGContextTranslateCTM (ctx, CGFloat (x) / sc, CGFloat (y + h) / sc);
    CGContextScaleCTM (ctx, 1.0, -1.0);
    CGContextSetInterpolationQuality (ctx, kCGInterpolationNone);
    CGContextDrawImage (ctx, CGRectMake (0, 0, CGFloat (w) / sc, CGFloat (h) / sc), image);
    CGContextRestoreGState (ctx);
    CGImageRelease (image);
}

- (NSPoint)localPoint:(NSEvent*)event
{
    return [self convertPoint:[event locationInWindow] fromView:nil];
}

- (void)mouseDown:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseDown (float (pt.x), float (pt.y), 0, modsFromEvent (event), int ([event clickCount]));
    [self pumpFrame];
}

- (void)rightMouseDown:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseDown (float (pt.x), float (pt.y), 1, modsFromEvent (event), int ([event clickCount]));
    [self pumpFrame];
}

- (void)mouseDragged:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseMove (float (pt.x), float (pt.y), modsFromEvent (event));
    [self pumpFrame];
}

- (void)rightMouseDragged:(NSEvent*)event
{
    [self mouseDragged:event];
}

- (void)mouseMoved:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseMove (float (pt.x), float (pt.y), modsFromEvent (event));
    [self pumpFrame];
}

- (void)mouseUp:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseUp (float (pt.x), float (pt.y), 0, modsFromEvent (event));
    [self pumpFrame];
}

- (void)rightMouseUp:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->mouseUp (float (pt.x), float (pt.y), 1, modsFromEvent (event));
    [self pumpFrame];
}

- (void)mouseExited:(NSEvent*)event
{
    (void)event;
    if (!_plugin || !_plugin->ui) return;
    _plugin->ui->mouseExit();
    [self pumpFrame];
}

- (void)scrollWheel:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    const bool precise = [event hasPreciseScrollingDeltas];
    float dx = float ([event scrollingDeltaX]);
    float dy = float ([event scrollingDeltaY]);
    _plugin->ui->wheel (float (pt.x), float (pt.y), dx, dy, precise, modsFromEvent (event));
    [self pumpFrame];
}

- (void)rotateWithEvent:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) return;
    NSPoint pt = [self localPoint:event];
    _plugin->ui->rotate (float (pt.x), float (pt.y), float ([event rotation]));
    [self pumpFrame];
}

- (void)keyDown:(NSEvent*)event
{
    if (!_plugin || !_plugin->ui) { [super keyDown:event]; return; }
    int k = -1;
    switch ([event keyCode])
    {
        case 123: k = 0; break; // Left
        case 124: k = 1; break; // Right
        case 126: k = 2; break; // Up
        case 125: k = 3; break; // Down
        case 36:  case 49: case 76: k = 4; break; // Return / Space / Enter
        case 53:  k = 5; break; // Escape
        default: break;
    }
    if (k >= 0 && _plugin->ui->key (k))
    {
        [self pumpFrame];
        return;
    }
    [super keyDown:event];
}

@end

namespace hdslave_desktop
{

struct PlatformGui
{
    DesktopPlugin* plugin = nullptr;
    HDSLAVE_OBJC_VIEW_CLASS* view = nil;
};

const char* platformGuiApiName()
{
    return CLAP_WINDOW_API_COCOA;
}

PlatformGui* createPlatformGui (DesktopPlugin* plugin)
{
    auto* g = new (std::nothrow) PlatformGui();
    if (!g) return nullptr;
    g->plugin = plugin;
    NSRect frame = NSMakeRect (0, 0, CGFloat (plugin->guiWidth), CGFloat (plugin->guiHeight));
    g->view = [[HDSLAVE_OBJC_VIEW_CLASS alloc] initWithPlugin:plugin frame:frame];
    return g;
}

void destroyPlatformGui (PlatformGui* gui)
{
    if (!gui) return;
    if (gui->view)
    {
        [gui->view stopTimer];
        [gui->view removeFromSuperview];
        [gui->view release];
        gui->view = nil;
    }
    delete gui;
}

bool platformGuiSetParent (PlatformGui* gui, const clap_window_t* window)
{
    if (!gui || !gui->view || !window || !window->cocoa)
        return false;
    NSView* parent = (__bridge NSView*) window->cocoa;
    [gui->view removeFromSuperview];
    [gui->view setFrame:NSMakeRect (0, 0, CGFloat (gui->plugin->guiWidth), CGFloat (gui->plugin->guiHeight))];
    [parent addSubview:gui->view];
    [gui->view syncSize];
    if (gui->plugin && gui->plugin->ui) gui->plugin->ui->invalidateAll();
    [gui->view startTimer];
    [gui->view pumpFrame];
    return true;
}

bool platformGuiShow (PlatformGui* gui)
{
    if (!gui || !gui->view) return false;
    [gui->view setHidden:NO];
    [gui->view setNeedsDisplay:YES];
    [gui->view syncSize];
    if (gui->plugin && gui->plugin->ui) gui->plugin->ui->invalidateAll();
    [gui->view startTimer];
    [gui->view pumpFrame];
    return true;
}

bool platformGuiHide (PlatformGui* gui)
{
    if (!gui || !gui->view) return false;
    [gui->view stopTimer];
    [gui->view setHidden:YES];
    return true;
}

bool platformGuiSetSize (PlatformGui* gui, uint32_t width, uint32_t height)
{
    if (!gui || !gui->view) return false;
    [gui->view setFrameSize:NSMakeSize (CGFloat (width), CGFloat (height))];
    return true;
}

} // namespace hdslave_desktop
