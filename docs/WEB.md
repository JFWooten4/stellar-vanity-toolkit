# Web generator: testing, self-hosting, and deployment

The `web/` directory is a **dependency-free static frontend**. It searches for
real Ed25519 Stellar keypairs with the browser's native Web Crypto API; it does
**not** use the Python/C programs on a server. It generates public G-addresses
and corresponding secret S-seeds. The first character after G must be A–D.
It searches for prefixes and/or suffixes, but not arbitrary embedded phrases.

## Threat model — read before using

- Every candidate is generated **in a browser Web Worker**. No API endpoint,
  wallet connection, analytics service, external JavaScript, font CDN, browser
  storage, cookies, server-side key generator, or logging of seeds is used.
- The worker exports a private Ed25519 seed **only on a full match**, verifies
  the RFC 8410 PKCS#8 header, and produces Stellar StrKey encoding with a
  CRC16-XModem checksum. The worker posts that result to the current page;
  both threads hold sensitive material in memory until discarded/terminated.
- Stopping terminates the worker. Discarding removes the page's references and
  rendered secret; **JavaScript cannot promise immediate physical zeroization
  of browser/OS memory**. Revealed seeds auto-hide after 30 seconds or when the
  tab becomes hidden. The seed remains in the page's memory until discarded.
- The production CSP forbids outgoing network connections and third-party
  scripts. This narrows exposure, but **does not defend against malicious
  hosting, modified HTML/JS, browser extensions, operating-system malware,
  screenshots, clipboard managers, developer tools, or compromised browsers**.
- Source code and deployments have **not undergone an independent audit**.
  A public URL is convenient for *test accounts and learning*, not a secure
  signing environment for significant value. Independently verify a returned
  seed derives the returned public address before funding it.
- Prefer the existing Python/C offline generators for serious experimentation,
  and reviewed hardware/offline key management for meaningful funds.
  Never paste secrets into a website or send them to the project maintainers.
- Vanity patterns are public identifiers, not extra entropy or security.
  A short match is normal; long matches may take impractically long.

## Run and test locally

Requires a recent browser implementing Web Crypto Ed25519, Web Workers and a
secure context. Browsers treat `http://localhost` / `http://127.0.0.1`
as potentially trustworthy for Web Crypto. **Opening `file://` will not
work** because the frontend loads a module worker. Python's built-in web
server is enough; the browser frontend does not require Python dependencies.

```bash
# From the repository root:
python3 -m http.server 4173 --bind 127.0.0.1 --directory web
# Open http://127.0.0.1:4173/
```

On Windows, replace `python3` with `py -3`. Stop with Ctrl+C.
Start with a one-character prefix (e.g., `A`) and validate the returned
G-address. Do **not** send generated secrets to other services to check them.
The simple Python server does not set the strict production security headers;
for a production-like local header test, use `vercel dev` or an HTTP server
configured with the headers in `vercel.json`.

With Node.js 20+ installed, run dependency-free encoding/crypto tests:

```bash
node --test tests/strkey.test.mjs
```

Manual browser acceptance checklist:

1. Enter `A` for the prefix and leave suffix empty. Verify a 56-character
   public G-address starting with `GA`; the private S-seed is masked.
2. Reveal/hide the seed, copy only after deliberate interaction, and discard.
3. Stop a long-running search; verify there is no displayed key and that the
   attempted count stops advancing.
4. Test invalid Base32 characters, a prefix starting `E`, and empty inputs.
5. In developer tools **Network**, confirm generation makes no requests
   after initial static assets load. In deployed environments verify the CSP
   response header, including `connect-src 'none'`.
6. Independently reproduce the public address from the secret on a trusted,
   offline Stellar SDK install; check against the result before relying on it.

## Self-host on a server

Serve **only the contents of `web/`** as static files over HTTPS. No database,
API backend, server key-generation process, or environment secrets are needed.
An ordinary static nginx, Caddy, Netlify, or Vercel site can serve it.

Use the security headers in `vercel.json` as the reference for your hosting
provider, particularly:

```text
Content-Security-Policy: default-src 'none'; base-uri 'none'; script-src 'self'; style-src 'self'; worker-src 'self'; connect-src 'none'; img-src 'self'; font-src 'none'; object-src 'none'; frame-ancestors 'none'; form-action 'none'; manifest-src 'none'
Cache-Control: no-store
X-Content-Type-Options: nosniff
Referrer-Policy: no-referrer
X-Frame-Options: DENY
```

Do not add analytics, externally hosted scripts, ad tags, proxy endpoints,
error reporters, CDN HTML injectors or wallet-secret upload features. Verify
the downloaded HTML/JS matches the audited Git revision.

## Vercel: `keygen.wooten.link`

`vercel.json` uses the repository root as the project root and sets the
output directory to `web`; Vercel serves **only** the static frontend.
There is no build command or serverless function, and no credentials should
be added as Vercel environment variables.

1. In Vercel, **Add New → Project**, import
   `JFWooten4/stellar-vanity-toolkit`. Select **Other** as the Framework
   Preset; leave the project Root Directory as `./`.
2. Deploy the branch as a **Preview** first. Check local-test steps above
   using Vercel's preview URL, and inspect response headers.
3. In **Project Settings → Domains**, add `keygen.wooten.link`.
   Check the **exact DNS values** Vercel gives this project (or run
   `vercel domains inspect keygen.wooten.link`). At your DNS provider create
   the specified CNAME for the `keygen` subdomain. Remove conflicting
   records. Wait for verification and an automatically issued HTTPS cert.
4. Make `main` the Vercel Production Branch if not already configured, and
   deploy to production **only after** the PR is reviewed and merged.
   Check `https://keygen.wooten.link` loads without an API, check the
   `Content-Security-Policy` header and HTTPS certificate, and confirm
   generated secrets never appear in network traffic.

### Proxy / DNS warning

A **custom domain is not an application proxy**: Vercel handles HTTPS and
serves static files at `keygen.wooten.link`. No `/api` proxy rewrite
is needed, and key material must never traverse one. If DNS is provided by
Cloudflare or another edge proxy, **prefer DNS-only** mode for this hostname.
A proxied configuration introduces another party that could alter the delivered
JavaScript. If proxying is unavoidable, require HTTPS to Vercel in
strict certificate-validation mode, disable script injection/optimization,
analytics and HTML rewriting, preserve all security headers, and document and
review the resulting additional trust boundary. Do not point an arbitrary
public reverse proxy at this app and describe it as zero-trust.

Reference: [Vercel project configuration](https://vercel.com/docs/project-configuration/vercel-json)
and [Vercel custom domains](https://vercel.com/docs/domains/set-up-custom-domain).
