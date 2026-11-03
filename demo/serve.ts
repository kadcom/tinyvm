// Local server for the demo page (bun demo/serve.ts [port]).
// Copyright (c) 2026 PT Baris Inovasi Ceria
// SPDX-License-Identifier: AGPL-3.0-only
//
// - demo/ files at /
// - the WebAssembly build (bazel build //:tinyvm_wasm) at /riscvemu64-wasm.{js,wasm}
// - /tinyemu/... forwarded to https://cdn.kadcom.dev/tinyemu/... so that the page and
//   the disk images share one origin (the CDN sends no CORS headers).
//   IMAGES_DIR=<dir> serves /tinyemu/... from a local directory instead.
import { join, normalize } from "node:path";

const port = Number(process.argv[2] ?? 8080);
const demoDir = import.meta.dir;
const wasmDir = join(demoDir, "..", "bazel-bin", "js");
const cdn = process.env.CDN ?? "https://cdn.kadcom.dev";
const imagesDir = process.env.IMAGES_DIR;

function fileUnder(root: string, rel: string): string | null {
  const p = normalize(join(root, rel));
  return p.startsWith(normalize(root)) ? p : null;
}

async function serveFile(path: string | null): Promise<Response> {
  if (path) {
    const f = Bun.file(path);
    if (await f.exists()) return new Response(f);
  }
  return new Response("not found", { status: 404 });
}

Bun.serve({
  port,
  async fetch(req) {
    const url = new URL(req.url);
    const path = decodeURIComponent(url.pathname);
    if (path.startsWith("/tinyemu/")) {
      if (imagesDir) return serveFile(fileUnder(imagesDir, path.slice("/tinyemu/".length)));
      const r = await fetch(cdn + path);
      const headers = new Headers(r.headers);
      // fetch() already decoded the body
      headers.delete("content-encoding");
      headers.delete("content-length");
      return new Response(r.body, { status: r.status, headers });
    }
    if (path === "/riscvemu64-wasm.js" || path === "/riscvemu64-wasm.wasm")
      return serveFile(join(wasmDir, path.slice(1)));
    return serveFile(fileUnder(demoDir, path === "/" ? "index.html" : path.slice(1)));
  },
});
console.log(`demo: http://localhost:${port}/`);
