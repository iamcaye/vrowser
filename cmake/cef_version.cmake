# Pinned official CEF binary distribution.
# Extended channel M156 (Chromium 156.0.8078.96). The Alloy bootstrap was
# removed in M128; this build uses the Chrome bootstrap and supports both
# Chrome-style and Alloy-style windows.
# Source: https://cef-builds.spotifycdn.com/index.html
# Channel notes: https://chromiumembedded.github.io/cef/branches_and_building.html

set(VROWSER_CEF_VERSION "156.0.30+g6028375+chromium-156.0.8078.96")
# Empty channel means the standard (non-beta) artifact name.
set(VROWSER_CEF_CHANNEL "")

set(VROWSER_CEF_SHA256_macosarm64
    "22a510e5d348a907953278b618e1e9e442ccf78773b9fee0e09eaf154eef53b0")
set(VROWSER_CEF_SHA256_macosx64
    "8b3b0cb92dc2f649a3561c6fc11351c10f9e589dc0536bd8635c2d6a73bc62b6")
set(VROWSER_CEF_SHA256_linux64
    "c55adc7b8c77bb1f4697ddc44421fbba862ae49275a05dfdc09e68af47c5a423")
set(VROWSER_CEF_SHA256_linuxarm64
    "ccf3bb2eec7a8adcb5667dd107c3ba0405350a8fcc4895d957e6c9dfb518c0c9")
set(VROWSER_CEF_SHA256_linuxarm
    "3cc6c315693739af57e9bd0712b5bff062d33a166906aebdb3dad9b3ca775593")
set(VROWSER_CEF_SHA256_windows64
    "bf99b029fc76b2e9fc2c35a9ea7385cf54d0b7119f42e49ab04e0eac81edaaf0")
set(VROWSER_CEF_SHA256_windowsarm64
    "70de6d5e8eeeaa022603cb167972e34fcb1983f87cd71d1716b7a9387c148528")
set(VROWSER_CEF_SHA256_windows32
    "ec3edcc4d1b0073d77aaf7fdfcfffe0c442f2d88f7629a4eb1c46727cfe60fc7")
