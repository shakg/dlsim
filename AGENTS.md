# AGENTS.md

## Branching and release flow

- `dev` is the integration branch. All work lands here first.
- `main` holds only tested, validated code. Releases are cut from `main`.

### Workflow

1. Branch from `dev` for each issue or feature (`feature/<short-name>`, `fix/<short-name>`).
2. Open the pull request against `dev`, never against `main`.
3. After merging to `dev`, build and test it:
   ```bash
   cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
   ```
   Also validate the change in the running app where it applies.
4. When `dev` is validated and ready to release, open a pull request from `dev` to `main`.
5. Release from `main`: set the version in `CMakeLists.txt` (`project(dlsim VERSION x.y.z)`), then tag `vx.y.z` on `main`. CI checks the tag matches the project version and builds the `.deb` and AppImage.

### Rules

- Don't commit directly to `dev` or `main`.
- Don't merge a feature branch straight into `main`.
- Delete feature branches after they merge.
