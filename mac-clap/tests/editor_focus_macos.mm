#import <Cocoa/Cocoa.h>
#include <clap/clap.h>

bool checkEditorFocus (const clap_plugin_t* plugin, const clap_plugin_gui_t* gui)
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        if (!gui->create (plugin, CLAP_WINDOW_API_COCOA, false)) return false;
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect (-3000, -3000, 960, 648)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        clap_window_t parent {};
        parent.api = CLAP_WINDOW_API_COCOA;
        parent.cocoa = window.contentView;
        bool ok = gui->set_parent (plugin, &parent) && gui->show (plugin);
        NSView* panel = window.contentView.subviews.lastObject;
        [window orderBack:nil];
        ok = ok && panel && [window makeFirstResponder:window];

        const NSEventType clicks[] = { NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp,
                                       NSEventTypeRightMouseDown, NSEventTypeRightMouseUp };
        for (NSEventType type : clicks)
        {
            if (!ok) break;
            NSEvent* event = [NSEvent mouseEventWithType:type location:NSMakePoint (880, 630)
                modifierFlags:0 timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:window.windowNumber
                context:nil eventNumber:1 clickCount:1 pressure:1];
            // Dispatch through AppKit so both automatic and explicit focus grabs are exercised.
            [NSApp postEvent:event atStart:YES];
            NSEvent* next = [NSApp nextEventMatchingMask:NSEventMaskFromType (type) untilDate:NSDate.distantPast
                inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!next || next.window != window) { ok = false; break; }
            [NSApp sendEvent:next];
            ok = window.firstResponder == window;
        }
        ok = ok && [window makeFirstResponder:panel] && window.firstResponder == panel;
        gui->destroy (plugin);
        [window close];
        [window release];
        return ok;
    }
}
