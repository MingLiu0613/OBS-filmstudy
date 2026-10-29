# Windows x64 M1 trial

This is an unsigned development build for local testing, not a Release. The source is task commit `941e9f61c86028489bb4e65dac2481a7c82a62bd`; the Windows package is the `filmstudy-windows-x64-941e9f61c86028489bb4e65dac2481a7c82a62bd` artifact from [Actions run 40892652246](https://github.com/MingLiu0613/OBS-filmstudy/actions/runs/40892652246). The runner reported `AMD64`. The outer artifact ZIP contains `obs-studio-33.0.0-windows-x64.zip`; extract both ZIP layers to a writable directory. Do not put footage or catalogs inside the extracted application directory.

## Start

1. Open a terminal in the extracted package's `bin\64bit` directory and run `.\obs64.exe --portable`. Dismiss OBS's optional first-run configuration wizard if it appears.
2. In OBS **Settings → Output**, use the normal **MPEG-4 (.mp4)** recording format. Disable **Automatically Remux to MP4**. Choose an output folder with free space. The Film Study entry currently expects OBS to finish an MP4 directly.
3. Add an OBS video source to the scene, such as Display Capture, Window Capture or Video Capture Device. Choose audio sources deliberately. For a first test, use a nonprivate source; the development build has no distribution signing.
4. Open **Tools → Basketball Film Study**. Choose a catalog directory outside the app folder, enter a game name, then select **Start Film Study recording**. The Film Study dialog hides while OBS records. Stop with OBS's normal **Stop Recording** button or recording hotkey. Wait for the Film Study status to report a saved Clip before analysing it.

## Analyse and find footage

1. Reopen **Tools → Basketball Film Study** if the dialog is hidden. Use **Add analysis to last saved clip** twice, giving the two analyses different perspective/topic pairs. The dialog supports adding analyses; editing an existing analysis is still pending.
2. Use **Search analysed clips**. Search for a perspective and topic from the same analysis; expect one Clip and the matching Analysis ID. Combine a perspective from one analysis with a topic from the other; expect zero results. This verifies that filters apply within one Analysis.
3. Use **Select saved clip** to inspect earlier saved Clips. **Preview last saved clip** opens an in-app preview after the file is ready. In the current test build, initial preview may stay black; press **Restart** to begin playback.
4. Use **Export analysed clips** to choose a matching perspective/topic and an output directory outside the catalog. The export contains the MP4 and a version 2 `manifest.json`. A large export blocks the current dialog until it finishes.

Keep the catalog, output MP4 and export package outside Git. To switch computers, push source changes through the task branch; move a test catalog only as user data with its `media/` directory. CI validates build and synthetic catalog behavior, while the native video recording check so far used a pure-color source with audio muted. Report actual screen/audio results separately.
