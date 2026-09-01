#include "PluginEditor.h"

#include "Diagnostics.h"

#include <IdmUI.h>

#include <array>
#include <cstring>

#if IDM_UI_PROBE
 #include <iostream>

namespace
{
    void probeLog (const juce::String& text)
    {
        std::cout << text << std::endl;

        const auto path = juce::SystemStats::getEnvironmentVariable ("IDM_UI_PROBE_OUT", {});

        if (path.isNotEmpty())
            juce::File (path).appendText (text + "\n");
    }
}

/// Clicking a control and reading the result back is the only way to tell a
/// theme switch that works from one that merely renders.
void IdmEditor::probeTheme()
{
    static constexpr const char* click = R"JS(
      (function () {
        var tab = Array.from(document.querySelectorAll('.vtab'))
                       .find(function (e) { return (e.textContent || '').trim() === 'Dark'; });
        if (!tab) { return 'no-dark-tab'; }
        tab.click();
        return 'clicked';
      })()
    )JS";

    static constexpr const char* read = R"JS(
      JSON.stringify({
        appClass:  (document.querySelector('.app') || {}).className || null,
        bodyClass: document.body.className || null,
        onTab:     (function () {
          var on = document.querySelector('.vtab--on');
          return on ? on.textContent.trim() : null;
        })(),
        windowInk: (function () {
          var w = document.querySelector('.uwin');
          return w ? getComputedStyle(w).backgroundColor : null;
        })()
      })
    )JS";

    const juce::Component::SafePointer<IdmEditor> safe { this };

    // Before, so the two can be compared rather than taken on trust.
    webView.evaluateJavascript (read, [safe] (auto before)
    {
        if (safe == nullptr)
            return;

        if (const auto* v = before.getResult())
            probeLog ("IDM_UI_PROBE theme=light " + v->toString());

        safe->webView.evaluateJavascript (click, [safe] (auto)
        {
            if (safe == nullptr)
                return;

            // React re-renders on its own schedule; give it a frame.
            juce::Timer::callAfterDelay (500, [safe]
            {
                if (safe == nullptr)
                    return;

                safe->webView.evaluateJavascript (read, [safe] (auto after)
                {
                    if (const auto* v = after.getResult())
                        probeLog ("IDM_UI_PROBE theme=dark " + v->toString());

                    if (safe == nullptr)
                    {
                        juce::JUCEApplicationBase::quit();
                        return;
                    }

                    // Press Start, then stay alive long enough for a listener
                    // on the virtual port to hear something. Proving notes
                    // leave the process needs the process to still be running.
                    static constexpr const char* press = R"JS(
                      (function () {
                        var b = Array.from(document.querySelectorAll('button'))
                                     .find(function (e) {
                                       var l = (e.getAttribute('aria-label') || e.title || '').toLowerCase();
                                       return l.indexOf('start') === 0;
                                     });
                        if (!b) { return 'no-start-button'; }
                        b.click();
                        return 'started';
                      })()
                    )JS";

                    safe->webView.evaluateJavascript (press, [safe] (auto pressed)
                    {
                        if (const auto* v = pressed.getResult())
                            probeLog ("IDM_UI_PROBE transport " + v->toString());

                        if (safe == nullptr)
                        {
                            juce::JUCEApplicationBase::quit();
                            return;
                        }

                        // Ask the interface to open an auxiliary window the way
                        // the Windows menu does. It draws on the canvas; there
                        // is no OS window to look for any more.
                        static constexpr const char* openAux = R"JS(
                          window.dispatchEvent(new CustomEvent('idm:open-window',
                                                               { detail: 'cyclic-editor' }));
                          'requested'
                        )JS";

                        safe->webView.evaluateJavascript (openAux, [safe] (auto)
                        {
                            juce::Timer::callAfterDelay (2500, [safe]
                            {
                                if (safe == nullptr)
                                {
                                    juce::JUCEApplicationBase::quit();
                                    return;
                                }

                                // Does the interface see what the engine played?
                                // This is what a user looks at to decide whether
                                // the plugin is doing anything.
                                static constexpr const char* monitor = R"JS(
                                  JSON.stringify({
                                    midiViewText: (function () {
                                      var el = document.querySelector('.mv__count, .mvcount')
                                            || Array.from(document.querySelectorAll('*'))
                                                   .find(function (e) {
                                                     return e.children.length === 0
                                                         && /messages?$/.test((e.textContent||'').trim());
                                                   });
                                      return el ? el.textContent.trim() : null;
                                    })(),
                                    rows: document.querySelectorAll('.midiview__row').length,
                                    noteBlocks: (function () {
                                      var n = document.querySelector('.midiview__notes');
                                      return n ? n.childElementCount : -1;
                                    })(),
                                    currentRowClass: (function () {
                                      var r = document.querySelector('.midiview__row.is-current, .midiview__row[data-current]');
                                      return r ? r.className : null;
                                    })(),
                                    transportOn: !!document.querySelector('.uconduct__tone--start.is-on, .is-playing')
                                  })
                                )JS";

                                safe->webView.evaluateJavascript (monitor, [] (auto seen)
                                {
                                    if (const auto* v = seen.getResult())
                                        probeLog ("IDM_UI_PROBE monitor " + v->toString());

                                    juce::Timer::callAfterDelay (3000, []
                                    {
                                        juce::JUCEApplicationBase::quit();
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    });
}

#endif

std::optional<juce::WebBrowserComponent::Resource>
IdmEditor::provide (const juce::String& path)
{
    // The bundle is one document, so every navigation resolves to it.
    //
    // Deliberately permissive. A pop-out loads the same bundle with a fragment
    // saying which window it is, and a provider that only answered "/" would
    // return nothing for anything else and the webview would show an error
    // frame instead of a window. Serving the document for any path is correct
    // here: there is only one document to serve.
    juce::ignoreUnused (path);

    int size = 0;

    if (const auto* data = IdmUI::getNamedResource ("index_html", size))
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);

        return juce::WebBrowserComponent::Resource { std::move (bytes),
                                                     "text/html; charset=utf-8" };
    }

    return std::nullopt;
}

IdmEditor::IdmEditor (IdmProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      webView (juce::WebBrowserComponent::Options {}
                   .withResourceProvider (provide)
                  #if JUCE_WINDOWS
                   // Windows' default backend is Internet Explorer, and JUCE's
                   // own header says it "will use an ancient version of IE".
                   // This interface is a modern bundle; it does not render
                   // there. Asking for webview2 is not enough on its own -
                   // JUCE silently falls back when the backend is unavailable,
                   // which is why prepareToPlay logs areOptionsSupported.
                   .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                  #endif
                   // Required before any native function can be called, and
                   // safe here because the only content loaded is our own
                   // bundle, served from memory.
                   .withNativeIntegrationEnabled()
                   .withNativeFunction ("setProject",
                                        [&p] (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete)
                                        {
                                            // The interface sends whole projects,
                                            // so a dropped intermediate costs
                                            // nothing and the engine can never
                                            // hold a half-applied edit.
                                            if (! args.isEmpty())
                                                p.setProjectFromJson (args[0].toString());

                                            complete (juce::var());
                                        })
                   .withNativeFunction ("poll",
                                        [this] (const juce::Array<juce::var>&,
                                                juce::WebBrowserComponent::NativeFunctionCompletion complete)
                                        {
                                            complete (pollEngine());
                                        })
                   .withNativeFunction ("setWindows",
                                        [&p] (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete)
                                        {
                                            if (! args.isEmpty())
                                                p.setWindowsJson (args[0].toString());

                                            complete (juce::var());
                                        })
                   .withNativeFunction ("setTransport",
                                        [&p] (const juce::Array<juce::var>& args,
                                              juce::WebBrowserComponent::NativeFunctionCompletion complete)
                                        {
                                            // Ignored in a plugin: the host owns
                                            // the transport there, and taking
                                            // this would give two of them.
                                            if (! args.isEmpty() && p.isStandalone())
                                                p.setStandaloneTransport ((bool) args[0]);

                                            complete (juce::var());
                                        }))
{

    addAndMakeVisible (webView);

    // Say which browser engine actually got used. On Windows the requested
    // webview2 backend falls back to Internet Explorer without complaint if the
    // WebView2 runtime is missing, and IE cannot render this interface - so the
    // symptom is a blank or broken window with nothing anywhere to explain it.
    // One line in the log turns that into a five-second diagnosis.
   #if JUCE_WINDOWS
    const auto backendAvailable = juce::WebBrowserComponent::areOptionsSupported (
        juce::WebBrowserComponent::Options {}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2));

    idm::Diagnostics::get().log (juce::String ("editor constructed  webview2=")
                                 + (backendAvailable ? "yes"
                                                     : "NO - falling back to Internet Explorer, "
                                                       "which cannot render this interface. Install "
                                                       "the Microsoft Edge WebView2 Runtime."));
   #else
    idm::Diagnostics::get().log ("editor constructed");
   #endif

   #if IDM_UI_PROBE
    probeLog ("IDM_UI_PROBE stage=editor-constructed");
    webView.onPageLoaded = [this]
    {
        probeLog ("IDM_UI_PROBE stage=page-loaded");
        probe (0);
    };
   #endif

    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    /*  Resizable, because the interface is a desktop of movable windows and
        1000 x 460 is not much of a desk.

        The web side already expects this: .workspace-viewport is
        `flex: 1 1 auto; overflow: auto`, so it fills whatever it is given and
        scrolls only when the content is genuinely larger. Nailing the editor
        shut was the only thing forcing everything to scroll inside a fixed box.

        The lower bound is the 640 x 480 the classic layout is drawn against,
        plus the menu bar - below that the windows start clipping rather than
        merely crowding. The upper bound is loose; a second monitor is a
        reasonable place to put this.
    */
    setResizable (true, true);
    setResizeLimits (640, 520, 4096, 2304);
    setSize (1000, 620);

    // The interface polls; nothing is pushed at it. See pollEngine.
}

/** Open one auxiliary window, or bring it forward if it is already open. */


void IdmEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

/** Everything the interface needs from the engine, in one answer.

    Pulled rather than pushed. JUCE's only push API is
    `emitEventIfBrowserIsVisible`, and the name is literal: inside a host the
    plugin editor's webview is not always considered visible, and the event is
    dropped with no error anywhere. A Midi View that silently stays empty in
    Ableton while working in the standalone is exactly that failure. A poll's
    reply comes back as the native call's own result, which has no such gate.
*/
juce::var IdmEditor::pollEngine()
{
    // Every reply to the interface, including this one, is delivered through
    // WebBrowserComponent::emitEventIfBrowserIsVisible - which drops it when
    // isVisible() is false and says nothing. If the interface looks dead inside
    // a host, this line is where to look first.
    idm::Diagnostics::get().logThrottled (
        "poll",
        "poll  webViewVisible=" + juce::String ((int) webView.isVisible())
        + "  editorVisible=" + juce::String ((int) isVisible())
        + "  showing=" + juce::String ((int) isShowing())
        + "  playing=" + juce::String ((int) processorRef.hostIsPlaying())
        + "  notesSent=" + juce::String (processorRef.notesEmitted()));

    auto* object = new juce::DynamicObject();

    // What the engine played, so Midi View can show it.
    {
        std::array<PlayedNote, 512> notes {};
        const auto count = processorRef.drainPlayed (notes.data(), (int) notes.size());

        juce::Array<juce::var> flat;
        flat.ensureStorageAllocated (count * 7);

        for (int i = 0; i < count; ++i)
        {
            const auto& n = notes[(size_t) i];
            flat.add (n.voice);
            flat.add (n.note);
            flat.add (n.velocity);
            flat.add (n.channel);
            flat.add (n.atTick);
            flat.add (n.durationTicks);
            flat.add (n.startSec);
        }

        object->setProperty ("played", flat);
    }

    // The host's MIDI, on its way to the Input Control System.
    {
        std::array<IncomingMidi, 256> messages {};
        const auto count = processorRef.drainIncoming (messages.data(), (int) messages.size());

        juce::Array<juce::var> flat;
        flat.ensureStorageAllocated (count * 3);

        for (int i = 0; i < count; ++i)
        {
            flat.add ((int) messages[(size_t) i].status);
            flat.add ((int) messages[(size_t) i].data1);
            flat.add ((int) messages[(size_t) i].data2);
        }

        object->setProperty ("midiIn", flat);
    }

    object->setProperty ("playing", processorRef.hostIsPlaying());
    object->setProperty ("tempo", processorRef.hostTempo());
    object->setProperty ("elapsed", processorRef.elapsedSeconds());

    // What the interface needs to answer "is this working" without a DAW's
    // help. Both plugin formats drop MIDI when the host declines the output,
    // and neither says so; a count the engine can vouch for is the difference
    // between diagnosing that and arguing about it.
    object->setProperty ("notesSent", processorRef.notesEmitted());
    object->setProperty ("port", processorRef.portName());
    object->setProperty ("standalone", processorRef.isStandalone());

    // A session the host restored, handed over once.
    if (processorRef.takeRestoredFlag())
    {
        object->setProperty ("document", processorRef.restoredDocument());
        object->setProperty ("windows", processorRef.restoredWindows());
    }

    // Windows closed by their own title bar, which the interface has no other
    // way of hearing about.

    return juce::var (object);
}



#if IDM_UI_PROBE

// Reports what the webview actually rendered, so "the UI loads" is a measured
// claim rather than an assumed one. Compiled out unless IDM_UI_PROBE is set.
void IdmEditor::probe (int attempt)
{
    static constexpr const char* script = R"JS(
      JSON.stringify({
        title: document.title,
        rootPresent: !!document.querySelector('.uroot'),
        windowCount: document.querySelectorAll('.uwin').length,
        windows: Array.from(document.querySelectorAll('.uwin__name'))
                      .map(function (n) { return n.textContent; }),
        stage: (function () {
          var s = document.querySelector('.ustage');
          if (!s) { return null; }
          var r = s.getBoundingClientRect();
          return Math.round(r.width) + 'x' + Math.round(r.height);
        })(),
        occupied: (function () {
          var ws = document.querySelectorAll('.uwin');
          if (!ws.length) { return null; }
          var l = Infinity, t = Infinity, r = -Infinity, b = -Infinity;
          ws.forEach(function (w) {
            var q = w.getBoundingClientRect();
            l = Math.min(l, q.left); t = Math.min(t, q.top);
            r = Math.max(r, q.right); b = Math.max(b, q.bottom);
          });
          return { left: Math.round(l), top: Math.round(t),
                   width: Math.round(r - l), height: Math.round(b - t) };
        })(),
        menubar: (function () {
          var m = document.querySelector('.app__menubar, .menubar, .app > header, .app__bar');
          if (m) { return m.className || m.tagName; }
          var t = document.querySelector('.vtab');
          return t ? 'vtab-row:' + (t.parentElement && t.parentElement.className) : null;
        })(),
        themeControls: Array.from(document.querySelectorAll('.vtab, .theme-picker'))
                            .map(function (e) { return (e.textContent || '').trim(); })
                            .filter(function (s) { return s.length > 0; }),
        themeClass: (document.querySelector('.app') || {}).className || null,
        viewport: window.innerWidth + 'x' + window.innerHeight,
        scrollable: document.documentElement.scrollWidth + 'x'
                  + document.documentElement.scrollHeight,
        fontsLoaded: document.fonts ? document.fonts.size : null,
        nodes: document.querySelectorAll('*').length
      })
    )JS";

    const juce::Component::SafePointer<IdmEditor> safe { this };

    webView.evaluateJavascript (script, [safe, attempt] (auto result)
    {
        if (safe == nullptr)
            return;

        juce::String line;

        if (const auto* value = result.getResult())
            line = value->toString();
        else if (const auto* error = result.getError())
            line = "{\"error\":\"" + error->message + "\"}";

        const auto rendered = line.contains ("\"windowCount\":")
                           && ! line.contains ("\"windowCount\":0");

        if (rendered || attempt >= 40)
        {
            probeLog ("IDM_UI_PROBE attempts=" + juce::String (attempt) + " " + line);
            probeLog ("IDM_UI_PROBE bridge projectsReceived="
                      + juce::String (safe->processorRef.projectsReceived())
                      + " liveVoices=" + juce::String (safe->processorRef.liveVoiceCount()));

            if (safe != nullptr)
                safe->probeTheme();

            return;
        }

        juce::Timer::callAfterDelay (250, [safe, attempt]
        {
            if (safe != nullptr)
                safe->probe (attempt + 1);
        });
    });
}

#endif
