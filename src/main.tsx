import React from "react";
import ReactDOM from "react-dom/client";
import { App } from "./ui/App";
import { installPluginBridge } from "./plugin/bridge";
import "./styles.css";

// No-op in a browser; inside the plugin it connects the windows to the engine.
installPluginBridge();

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
