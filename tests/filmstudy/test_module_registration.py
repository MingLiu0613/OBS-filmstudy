"""Verify that the built OBS library registers the Film Study core module."""

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("obs_library", type=Path)
    args = parser.parse_args()

    library = args.obs_library.read_bytes()
    module_name = b"filmstudy-ui\0"
    if module_name not in library:
        raise SystemExit(
            f"{args.obs_library}: Film Study core module is absent from the compiled OBS registry"
        )
    print(f"Film Study core module registered in {args.obs_library}")


if __name__ == "__main__":
    main()
