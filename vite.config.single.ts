// One self-contained HTML file: no server, no install, double-click to run.
//
// This is idM's most useful distribution format short of the plugin —
// the whole instrument inlined into a single file that runs from a USB stick
// or an email attachment, which is a real thing to be able to hand somebody.
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { viteSingleFile } from "vite-plugin-singlefile";

export default defineConfig({
  base: "./",
  plugins: [react(), viteSingleFile()],
  build: {
    outDir: "dist-single",
    emptyOutDir: true,
  },
});
