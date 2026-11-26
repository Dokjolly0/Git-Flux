#!/usr/bin/env python3
"""
Generate high-resolution (64x64) transparent PNG icons for Git providers
used in the Clone Repository modal.

Requires ImageMagick (`magick`) with librsvg support.
"""
import subprocess
import tempfile
from pathlib import Path

SVGS = {
    # Official GitHub Invertocat (light fill for dark UI)
    "github": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path fill="#F0F6FC" d="M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12"/>
</svg>""",

    # Official Atlassian Bitbucket mark (two-tone blue with clipped gradient shadow)
    "bitbucket": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="bb-grad" x1="100%" y1="25%" x2="42%" y2="85%">
      <stop offset="18%" stop-color="#0052CC"/>
      <stop offset="100%" stop-color="#2684FF"/>
    </linearGradient>
    <clipPath id="bb-clip">
      <path d="M2.008 0C.898 0 .001.898 0 2.008c0 .108.009.216.026.322L8.48 53.65c.214 1.304 1.339 2.262 2.66 2.266h40.42c1.006.013 1.869-.714 2.03-1.707l8.454-51.878c.18-.98-.47-1.92-1.45-2.1-.107-.02-.215-.03-.324-.03H2.008zm35.515 37.05H24.688l-3.44-18.02h19.675l-3.4 18.02z"/>
    </clipPath>
  </defs>
  <g transform="translate(1, 4)">
    <path fill="#2684FF" d="M2.008 0C.898 0 .001.898 0 2.008c0 .108.009.216.026.322L8.48 53.65c.214 1.304 1.339 2.262 2.66 2.266h40.42c1.006.013 1.869-.714 2.03-1.707l8.454-51.878c.18-.98-.47-1.92-1.45-2.1-.107-.02-.215-.03-.324-.03H2.008zm35.515 37.05H24.688l-3.44-18.02h19.675l-3.4 18.02z"/>
    <path clip-path="url(#bb-clip)" fill="url(#bb-grad)" d="M59.6 19.03H40.923l-3.4 18.02H24.688L8.48 55.916h45.5L59.6 19.03z"/>
  </g>
</svg>""",

    # Official GitLab Tanuki logo (3-tone red-orange, orange, amber)
    "gitlab": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 380 380">
  <g transform="translate(0, 12)">
    <path fill="#E24329" d="M190 344.2L254.3 146.3H125.7L190 344.2z"/>
    <path fill="#FC6D26" d="M190 344.2L125.7 146.3H35.6L190 344.2z"/>
    <path fill="#FCA326" d="M35.6 146.3L16 206.5c-1.8 5.5.2 11.5 4.9 14.9L190 344.2 35.6 146.3z"/>
    <path fill="#E24329" d="M35.6 146.3h90.1L87 27.1c-2-6.1-10.6-6.1-12.6 0L35.6 146.3z"/>
    <path fill="#FC6D26" d="M190 344.2l64.3-197.9h90.1L190 344.2z"/>
    <path fill="#FCA326" d="M344.4 146.3L364 206.5c1.8 5.5-.2 11.5-4.9 14.9L190 344.2l154.4-197.9z"/>
    <path fill="#E24329" d="M344.4 146.3h-90.1L293 27.1c2-6.1 10.6-6.1 12.6 0l38.8 119.2z"/>
  </g>
</svg>""",

    # Official Gitea teacup + git branch logo
    "gitea": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <path fill="#609926" d="M10 16h41.5c5.8 0 10.5 4.6 10.5 10.4 0 5.4-4.2 9.9-9.5 10.3-.4 1.4-.9 2.7-1.6 4C47.5 47.7 41.8 52 35 52H22.2C13 52 6 43 6 31V20c0-2.2 1.8-4 4-4zm41.5 6.4H49v8.4h2.5c2.4 0 4.3-1.9 4.3-4.2 0-2.3-1.9-4.2-4.3-4.2z"/>
  <rect x="20.5" y="24" width="17" height="17" rx="3.2" transform="rotate(45 29 32.5)" fill="#FFFFFF"/>
  <g stroke="#609926" stroke-width="2.2" stroke-linecap="round" fill="none">
    <line x1="27.2" y1="26.5" x2="27.2" y2="37.8"/>
    <path d="M27.2 29.8 L32.2 33.8"/>
  </g>
  <circle cx="27.2" cy="26.2" r="2.1" fill="#609926"/>
  <circle cx="27.2" cy="38.2" r="2.1" fill="#609926"/>
  <circle cx="32.5" cy="34.0" r="2.1" fill="#609926"/>
</svg>""",

    # Official Forgejo mark (orange + red forged branches)
    "forgejo": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke-linecap="round" stroke-linejoin="round" stroke-width="6">
    <path d="M20 48V24c0-6.6 5.4-12 12-12h12" stroke="#FB923C"/>
    <path d="M20 48V38c0-6.6 5.4-12 12-12h12" stroke="#D40000"/>
  </g>
  <circle cx="44" cy="12" r="5.5" fill="#0E1420" stroke="#FB923C" stroke-width="4.2"/>
  <circle cx="44" cy="26" r="5.5" fill="#0E1420" stroke="#D40000" stroke-width="4.2"/>
  <circle cx="20" cy="50" r="5.5" fill="#0E1420" stroke="#D40000" stroke-width="4.2"/>
</svg>""",

    # Official Codeberg mark (blue iceberg + circular frame)
    "codeberg": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <defs>
    <linearGradient id="cb-grad" x1="35%" y1="22%" x2="65%" y2="96%">
      <stop offset="0%" stop-color="#2185D0" stop-opacity="0.18"/>
      <stop offset="50%" stop-color="#2185D0" stop-opacity="0.78"/>
      <stop offset="100%" stop-color="#2185D0"/>
    </linearGradient>
  </defs>
  <path fill="#2185D0" d="M32 4C16.536 4 4 16.536 4 32c0 6.8 2.43 13.03 6.47 17.88l20.23-26.2c.65-.84 1.95-.84 2.6 0l20.23 26.2C57.57 45.03 60 38.8 60 32 60 16.536 47.464 4 32 4zm0 5.4c12.48 0 22.6 10.12 22.6 22.6 0 4.2-1.15 8.13-3.15 11.5L34.7 21.8c-1.35-1.75-4.05-1.75-5.4 0L12.55 43.5C10.55 40.13 9.4 36.2 9.4 32 9.4 19.52 19.52 9.4 32 9.4z"/>
  <path fill="url(#cb-grad)" d="M32 23.2L53.2 50.6C48 56.4 40.4 60 32 60c-4.4 0-8.55-1-12.25-2.8L32 23.2z"/>
</svg>""",

    # Generic self-hosted Git server icon
    "server": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="8" y="10" width="48" height="18" rx="4.5" fill="#1E293B" stroke="#94A3B8" stroke-width="3.5"/>
  <circle cx="18" cy="19" r="3" fill="#38BDF8"/>
  <line x1="27" y1="19" x2="47" y2="19" stroke="#94A3B8" stroke-width="3.5" stroke-linecap="round"/>
  <rect x="8" y="36" width="48" height="18" rx="4.5" fill="#1E293B" stroke="#94A3B8" stroke-width="3.5"/>
  <circle cx="18" cy="45" r="3" fill="#10B981"/>
  <line x1="27" y1="45" x2="47" y2="45" stroke="#94A3B8" stroke-width="3.5" stroke-linecap="round"/>
</svg>""",

    # Direct URL chain-link icon
    "url": """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <g fill="none" stroke="#38BDF8" stroke-width="5.2" stroke-linecap="round" stroke-linejoin="round">
    <path d="M27.5 36.5a11.5 11.5 0 0 0 16.3 0l7.5-7.5a11.5 11.5 0 0 0-16.3-16.3l-4.2 4.2"/>
    <path d="M36.5 27.5a11.5 11.5 0 0 0-16.3 0l-7.5 7.5a11.5 11.5 0 0 0 16.3 16.3l4.2-4.2"/>
  </g>
</svg>""",
}


def generate_provider_icons() -> None:
    repo_root = Path(__file__).resolve().parent.parent.parent
    out_dir = repo_root / "assets" / "icons" / "providers"
    out_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        for name, svg in SVGS.items():
            svg_path = tmp_dir / f"{name}.svg"
            png_path = out_dir / f"{name}.png"
            svg_path.write_text(svg, encoding="utf-8")
            subprocess.run(
                [
                    "magick",
                    "-background", "none",
                    "-density", "384",
                    f"RSVG:{svg_path}",
                    "-resize", "64x64",
                    "-gravity", "center",
                    "-extent", "64x64",
                    str(png_path),
                ],
                check=True,
            )
            print(f"Generated {png_path.relative_to(repo_root)}")


if __name__ == "__main__":
    generate_provider_icons()
