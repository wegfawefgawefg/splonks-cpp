# Android Play Release Handoff

This is the manual Play Console handoff for a real Splonks Android release. It
starts after the local signed AAB has passed `verify_release_aab.sh`.

Normal branch pushes must not upload to Google Play. Use this path only for an
intentional manual or tagged release.

Google references:

- Upload signed app bundles from Android Studio or command-line builds:
  <https://developer.android.com/studio/publish/upload-bundle>
- Prepare and roll out a Play Console release:
  <https://support.google.com/googleplay/android-developer/answer/9859348>
- Set up internal, closed, or open testing:
  <https://support.google.com/googleplay/android-developer/answer/9845334>

## Build And Verify

Use the real upload keystore, not the throwaway local validation key:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
export SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_ANDROID_VERSION_CODE=1
export SPLONKS_ANDROID_VERSION_NAME=0.1.0
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
./scripts/android/verify_release_aab.sh
```

Expected files:

```text
dist/splonks-android/splonks-0.1.0-android-release.aab
dist/splonks-android/manifest.txt
```

Record the manifest contents and SHA-256 before upload:

```bash
cat dist/splonks-android/manifest.txt
```

## Internal Testing Upload

Use internal testing first for every Play release candidate:

1. Open Play Console.
2. Select the Splonks app.
3. Go to `Test and release` > `Testing` > `Internal testing`.
4. Create a new release.
5. Upload `dist/splonks-android/splonks-<version>-android-release.aab`.
6. Confirm Play Console accepts the bundle, package name, version code, and
   signing certificate.
7. Add release notes.
8. Review the release.
9. Roll out to internal testing.
10. Install from the tester link on a physical Android device.
11. Launch the game and record whether startup, rendering, audio, settings
    writes, and a short gameplay smoke work.

Evidence to record in `dist/validation/` or the release notes:

- Play Console track: internal testing.
- Version code and version name.
- AAB filename and SHA-256 from `manifest.txt`.
- Play Console upload result.
- Any policy, target API, signing, or permissions warnings.
- Tester link or release id, if available.
- Device model, Android version, and smoke result.

Command-line upload path:

```bash
gem install fastlane
export SPLONKS_PLAY_SERVICE_ACCOUNT_JSON=/absolute/path/to/google-play-service-account.json
export SPLONKS_ANDROID_PACKAGE_NAME=dev.splonks.game
export SPLONKS_PLAY_TRACK=internal
export SPLONKS_PLAY_RELEASE_STATUS=draft
./scripts/android/upload_play.sh
```

The upload helper verifies the exact AAB first, then runs `fastlane supply` and
writes a timestamped `dist/validation/android-play-*.log`. It defaults to
`draft` so an upload can be reviewed in Play Console before rollout. For a
validation-only API call, use:

```bash
./scripts/android/upload_play.sh --validate-only
```

Validation-only logs are recorded as `[play-upload] validate-only complete` and
do not satisfy the final Play upload gate. Release completion requires a
non-validate-only log with `[play-upload] upload complete`.

To make the release available to internal testers from the command line, set
`SPLONKS_PLAY_RELEASE_STATUS=completed` intentionally before running the helper.

GitHub Actions upload path:

- Manual dispatch: run `package`, set `release_version`, and enable
  `upload_android_play`.
- Tagged release: set repository variable `SPLONKS_UPLOAD_ANDROID_PLAY=true`
  only for tag releases that should upload to Google Play.

Required GitHub secrets/variables:

```text
SPLONKS_ANDROID_KEYSTORE_BASE64          secret
SPLONKS_ANDROID_KEYSTORE_PASSWORD        secret
SPLONKS_ANDROID_KEYSTORE_TYPE            repository variable, default jks
SPLONKS_ANDROID_KEYSTORE_PURPOSE         repository variable, must be upload
SPLONKS_ANDROID_KEY_ALIAS                secret
SPLONKS_ANDROID_KEY_PASSWORD             secret
SPLONKS_PLAY_SERVICE_ACCOUNT_JSON_BASE64 secret
SPLONKS_ANDROID_PACKAGE_NAME             repository variable
SPLONKS_PLAY_TRACK                       repository variable, default internal
SPLONKS_PLAY_RELEASE_STATUS              repository variable, default draft
```

The workflow decodes the service account JSON into the runner temp directory,
runs `release_credentials_preflight.sh android-play` to check the Play
credentials and the signed AAB/manifest for the selected version, then uses the
same `scripts/android/upload_play.sh` helper as the local path.

Evidence helper:

```bash
./scripts/validate_platform.sh android-play-upload
./scripts/bundle_validation_evidence.sh android-play
```

## Production Upload

Only move to production after internal testing has passed.

1. Create or promote a release in the production track.
2. Confirm the same version code/version name and artifact SHA-256 are being
   used.
3. Complete any Play Console review requirements, declarations, store listing
   checks, country availability, and content rating items.
4. Review release warnings.
5. Start rollout when ready.
6. Record the rollout status and release id.

## Completion Gate

Android release distribution is complete only when:

- `build_release_aab.sh` passes with the real upload key.
- `verify_release_aab.sh` passes on the exact AAB uploaded to Play Console.
- Play Console accepts the AAB on internal testing.
- A tester can install the Play-delivered build on a physical Android device.
- The Play-delivered build launches and passes a short runtime smoke.
- Production rollout steps are documented for the release when we are ready to
  publish publicly.
