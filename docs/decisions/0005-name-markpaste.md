# 0005. The name Markpaste and the ID su._525.Markpaste

- Status: accepted (2026-10-19)
- Spec items: section 2 (working name), section 11 (application ID), section 14 (open questions)

## Context

The spec gave the working name Pointshot and asked to choose the final one before the first public release. A check on 2026-10-19 found problems with Pointshot:

- the name says nothing about the main thing, pasting the set into a chat;
- there is a browser extension "Point & Shoot" with a very similar idea (interface screenshots with notes);
- the `github.com/pointshot` account is taken, so the ID `io.github.pointshot.Pointshot` would point to someone else's account;
- a search for the word returns the "point shooting" firing technique.

## Decision

- **The name is Markpaste**: "mark → paste", both steps of the scenario. As of 2026-10-19: the `github.com/markpaste` account is free, there are 4 little-known repositories with a similar name, and Fedora has no such package.
- **The ID is `su._525.Markpaste`.** The repository lived on `gitlab.525.su`, the domain belongs to the author, and the ID is the reversed domain. An ID element cannot start with a digit (D-Bus and Desktop Entry specifications), so `525` is written as `_525`. The KDE shortcuts component is `su._525.Markpaste`, the D-Bus object is `su__525_Markpaste`.
- **Executable `markpaste`**, settings directory `~/.config/markpaste/`, temporary files `$XDG_RUNTIME_DIR/markpaste/`, translations `markpaste_*.ts`, C++ namespace `mp`, CMake variables `MARKPASTE_*`.
- The name and ID are set in one place, at the top of `CMakeLists.txt`.

## Options considered

- Shotpaste, Snipnote, Pinshot, Shotlist: the GitHub accounts are taken, Pinshot and Shotlist have many repositories with the same name.
- `io.github.markpaste.Markpaste`: the GitHub account is free but does not belong to the author.

## Consequences

- After the rename KDE asks for permissions again: for global hotkeys on the first start, for keyboard control on the first paste. Portal permissions are tied to the ID.
- **The ID does not change after the first public release** (spec section 11).
- Before publishing, the name should also be checked in Debian, on Flathub and among trademarks: so far only GitHub and Fedora were checked.
- Update 2026-10-21: the project is published as `github.com/metall1c/markpaste` ([ADR 0024](0024-github-home-and-releases.md)). The ID stays `su._525.Markpaste`: the 525.su domain remains the author's.
