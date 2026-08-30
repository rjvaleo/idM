#include "PluginEditor.h"

#include <MClassicUI.h>

#include <cstring>

#if MCLASSIC_UI_PROBE
 #include <iostream>

namespace
{
    void probeLog (const juce::String& text)
    {
        std::cout << text << std::endl;

        const auto path = juce::SystemStats::getEnvironmentVariable ("MCLASSIC_UI_PROBE_OUT", {});

        if (path.isNotEmpty())
            juce::File (path).appendText (text + "\n");
    }
}

/// Clicking a control and reading the result back is the only way to tell a
/// theme switch that works from one that merely renders.
void MClassicEditor::probeTheme()
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

    const juce::Component::SafePointer<MClassicEditor> safe { this };

    // Before, so the two can be compared rather than taken on trust.
    webView.evaluateJavascript (read, [safe] (auto before)
    {
        if (safe == nullptr)
            return;

        if (const auto* v = before.getResult())
            probeLog ("MCLASSIC_UI_PROBE theme=light " + v->toString());

        safe->webView.evaluateJavascript (click, [safe] (auto)
        {
            if (safe == nullptr)
                return;

            // React re-renders on its own schedule; give it a frame.
            juce::Timer::callAfterDelay (500, [safe]
            {
                if (safe == nullptr)
                    return;

                safe->webView.evaluateJavascript (read, [] (auto after)
                {
                    if (const auto* v = after.getResult())
                        probeLog ("MCLASSIC_UI_PROBE theme=dark " + v->toString());

                    juce::JUCEApplicationBase::quit();
                });
            });
        });
    });
}

#endif

std::optional<juce::WebBrowserComponent::Resource>
MClassicEditor::provide (const juce::String& path)
{
    // The bundle is one document; every navigation resolves to it.
    if (path != "/" && path != "/index.html")
        return std::nullopt;

    int size = 0;

    if (const auto* data = MClassicUI::getNamedResource ("index_html", size))
    {
        std::vector<std::byte> bytes ((size_t) size);
        std::memcpy (bytes.data(), data, (size_t) size);

        return juce::WebBrowserComponent::Resource { std::move (bytes),
                                                     "text/html; charset=utf-8" };
    }

    return std::nullopt;
}

MClassicEditor::MClassicEditor (MClassicProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      webView (juce::WebBrowserComponent::Options {}
                   .withResourceProvider (provide))
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (webView);

   #if MCLASSIC_UI_PROBE
    probeLog ("MCLASSIC_UI_PROBE stage=editor-constructed");
    webView.onPageLoaded = [this]
    {
        probeLog ("MCLASSIC_UI_PROBE stage=page-loaded");
        probe (0);
    };
   #endif

    webView.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    setSize (1000, 460);
    setResizable (false, false);
}

void MClassicEditor::resized()
{
    webView.setBounds (getLocalBounds());
}

#if MCLASSIC_UI_PROBE

// Reports what the webview actually rendered, so "the UI loads" is a measured
// claim rather than an assumed one. Compiled out unless MCLASSIC_UI_PROBE is set.
void MClassicEditor::probe (int attempt)
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

    const juce::Component::SafePointer<MClassicEditor> safe { this };

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
            probeLog ("MCLASSIC_UI_PROBE attempts=" + juce::String (attempt) + " " + line);

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
