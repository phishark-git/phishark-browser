# SPDX-License-Identifier: GPL-3.0-only
"""Product-facing GRIT text branding, without rewriting links or legal notices."""
import re

def brand_message(original, upstream, name):
    if not name:
        return upstream
    if any(word in name.upper() for word in ('COPYRIGHT', 'LICENSE', 'CREDITS', 'TERMS_OF', 'PRIVACY_POLICY', 'LEGAL', 'TRADEMARK')):
        return original.replace('dontreplace', '')
    # These refer to another product/service, not the installed browser.
    if any(word in name.upper() for word in ('WEB_STORE', 'CHROMECAST', 'CHROME_OS', 'CHROMEBOOK')):
        return upstream
    parts = re.split(r'(https?://[^\s<>"\']+|chrome://[^\s<>"\']+)', upstream)
    for index in range(0, len(parts), 2):
        parts[index] = re.sub(r'\b[Cc]romite\b', 'PhiShark Browser', parts[index])
    return ''.join(parts)
