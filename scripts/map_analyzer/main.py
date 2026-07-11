"""Entry point shim — delegates to analyze_map.main."""

from analyze_map import main

if __name__ == "__main__":
    raise SystemExit(main())
