# OpenRC Release Process

OpenRC will follow [semantic versioning](https://semver.org/) once we
move to 1.0.0.

Major and Minor releases will be created on the master branch, and patch
releases will be created on the branch for the appropriate major or minor release.

OpenRC releases will be created by the primary OpenRC developers, which
are currently [navi](https://github.com/navi-desu) and
[williamh](https://github.com/williamh).

Here are the steps we use to create a release.

## Major or Minor release

A major or minor release is directly on the master branch, so make sure
changes you do not want in the release are not merged into it.

Once all changes you want in the release are committed and pushed on the
master branch, follow these steps:

- Update the news file then commit and push.
- Update the version in the top level meson.build then commit and push.
- Tag the release  with a signed tag as follows:

```
$ git tag -s -m 'release major.minor' major.minor
```

- push the tag

## patch release

Patch releases are created on branches based on the major or minor
release they are related to. Other than the version update in the top
level meson.build, patch releases should only contain commits
cherry-picked from master after the major/minor release they are related
to. Patch releases are for bug fixes only; new features should not be
added without going to a new major or minor release.

Here are the steps for creating a patch release.

- make sure you have the patch release branch created and checked out.
  Patch release branches are created with names like 1.1.x. If the
  patch release branch you need does not exist, create it as follows:

  ```
  git checkout -b 1.1.x 1.1
  git push -u origin 1.1.x
  ```

  - Check out the patch release branch:

```
git checkout 1.1.x
```

- use git-cherry(1) or some other method to determine which commits to
  cherry-pick from master for the patch release.
- use git cherry-pick(1) to copy commits from master to the patch
  release branch.  If the cherry-pick fails, the commit should not be used in the patch release.
- Optionally update the news file then commit and push.
- Update the version in the top level meson.build then commit and push.
- Tag the release  with a signed tag as follows:

```
$ git tag -s -m 'release major.minor.patch' major.minor.patch
```

- push the tag
