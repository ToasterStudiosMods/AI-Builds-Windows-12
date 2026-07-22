# Aurelian OS Blueprint

Aurelian OS is a fully original, Windows 12-inspired desktop operating system concept for x86-64 PCs and virtual machines. It is designed as a bootable ISO distribution with a polished desktop shell, modern installer, resilient update model, and practical hardware support strategy while avoiding Microsoft code, branding, names, icons, sounds, and proprietary assets.

## Project overview

**Suggested codename:** `Aurelian`.

Aurelian OS is a desktop-first operating system built around an original kernel, original desktop environment, installer, system applications, update stack, and design language. The v1 implementation targets UEFI x86-64 machines and common virtual machines. Legacy BIOS support is treated as feasible but optional because modern PCs and secure boot flows are UEFI-centered.

The product is distributed as:

- A bootable hybrid ISO for optical media, hypervisors, and USB imaging.
- An optional USB installer image with persistence for diagnostics.
- Signed release artifacts with checksums, SBOMs, and reproducible build metadata.

The system should feel familiar to users of modern desktop operating systems without copying product identity, implementation, or assets from any proprietary platform.

## Product goals and target users

### Goals

- Install successfully on bare-metal x86-64 PCs and common virtual machines.
- Boot quickly, remain responsive under load, and keep idle memory low.
- Offer a friendly consumer mode and a power-user mode with deeper controls.
- Provide a coherent desktop with centered launcher, translucent surfaces, rounded windows, smooth animation, and accessible contrast options.
- Support offline installation with hardware detection and post-install driver/package recommendations.
- Make maintenance, recovery, rollback, and diagnostics understandable to non-experts.

### Target users

- Everyday desktop users who want a polished, familiar PC experience.
- Power users who want terminal access, detailed diagnostics, virtualization-friendly behavior, and transparent system state.
- Developers who need stable tooling, containers, package repositories, and fast multitasking.
- Testers and educators who need a VM-friendly desktop OS blueprint.

## UI/UX design language

### Brand and visual identity

- **Product name:** Aurelian OS.
- **Shell name:** Luma Shell.
- **Design system:** Prism UI.
- **Default wallpaper style:** Abstract layered glass ribbons over soft radial gradients, generated from original vector assets.
- **Icon style:** Original rounded-square glyphs with two-tone depth, consistent stroke weights, and no Microsoft icon derivatives.
- **Sound theme:** Original short synthetic chimes, muted by default during install.
- **Fonts:** Inter or Noto Sans for open-source Latin coverage, Noto Sans CJK/Arabic/Hebrew optional language packs, JetBrains Mono for terminal.

### Desktop shell

- **Centered taskbar/dock:** Luma Dock sits at the bottom by default, centered with pinned apps, running indicators, and adaptive overflow. It can move left, right, or top in power-user mode.
- **Start menu:** The `Launchpad` opens above the dock with pinned apps, recent files, app categories, search, shutdown actions, and account controls.
- **Panels:** Translucent acrylic-like panels use GPU blur when available and fall back to opaque panels on low-end hardware or accessibility high-contrast mode.
- **Windows:** Rounded corners, soft shadows, snap layouts, keyboard-driven tiling, and subtle focus rings.
- **Animation:** 120-220 ms easing for launch, minimize, panel open, and workspace switching. Animation can be reduced or disabled.
- **Themes:** Light, dark, automatic sunrise/sunset, high contrast, color-blind-friendly accent palettes, and large text presets.

### Interaction model

- Primary search shortcut opens universal search across apps, files, settings, commands, and recent documents.
- Quick Settings contains Wi-Fi, Bluetooth, airplane mode, brightness, volume, battery, VPN, casting, night light, focus mode, and power profile controls.
- Notification Center groups alerts by app, includes inline actions, and pairs with a calendar/agenda pane.
- Consumer mode hides low-level system controls; power-user mode reveals logs, services, startup entries, repositories, and kernel options.

## Feature list by category

### Desktop and productivity

- Launchpad app launcher with pinned apps, categories, recent files, and search indexing.
- File manager with tabs, breadcrumbs, quick access, split view, drag-and-drop, archive browsing, network shares, metadata preview, and undo for file operations.
- Virtual desktops with named workspaces and per-workspace wallpapers in v2.
- Clipboard history with searchable text/image snippets and privacy exclusions for password fields.
- Snipping and screen recording utility using compositor capture portals.
- Tabbed terminal with profiles, split panes, command palette, and SSH bookmark support.
- Task manager with process tree, resource graphs, startup apps, services, GPU, network, disks, and per-app power usage.
- Per-app audio mixer backed by PipeWire routing.

### Connectivity and devices

- Wi-Fi and Bluetooth controls in Quick Settings and Settings.
- Printer discovery through IPP Everywhere, DNS-SD, and CUPS.
- Network browsing for SMB, SFTP, and WebDAV.
- Driver manager for firmware, kernel modules, GPU stack status, and proprietary-driver recommendations when legally distributable.
- Remote desktop/screen sharing through Wayland remote desktop portals and optional RDP/VNC compatibility.

### System management

- Settings app with searchable categories: System, Personalization, Apps, Accounts, Network, Bluetooth, Privacy, Security, Updates, Recovery, Accessibility, Time & Language, Gaming, Developer.
- File association manager with default app controls and MIME handlers.
- Backup and restore utility for home folders, settings, app list, and optional full-system snapshots.
- Restore points backed by Btrfs snapshots where available; ext4 installations use rsync/restic-style restore points for selected paths.
- Software Center for approved repositories, Flatpak-style sandboxed apps, system packages, updates, reviews, and parental/safety metadata.

### Built-in apps

- File manager, browser shell, terminal, text editor, calculator, media player, image viewer, archive manager, screenshot/recorder, system monitor, settings, software center, backup tool, password manager, Bluetooth/Wi-Fi tools, and PDF viewer.

## System architecture

### Kernel and base system

- **v1 kernel choice:** Build an original x86-64 kernel named `Aurelion`, not a Linux fork. The kernel is clean-room project code using public CPU, ACPI, PCI, USB, NVMe, AHCI, FAT, ext4, and networking specifications, permissively licensed dependencies where appropriate, and no proprietary Microsoft implementation details.
- **Kernel style:** Hybrid modular kernel. Core scheduling, memory management, IPC, object handles, security labels, and VFS run in kernel mode; high-risk subsystems such as printer stacks, Bluetooth policy, most filesystem repair tooling, and complex device services run as isolated user-mode servers.
- **Boot ABI:** UEFI loads a signed bootloader, which loads `aurelion.elf`, initrd modules, kernel command line, ACPI tables, framebuffer details, memory map, and measured-boot events using a documented `AURELION_BOOT_INFO` structure. Optional BIOS support uses a compatibility loader that builds the same boot-info structure.
- **CPU target:** x86-64-v2 baseline where possible, with compatibility builds for older x86-64 CPUs if project goals require it. Initial CPU support includes long mode, SMP startup, APIC/x2APIC, HPET/TSC deadline timers, XSAVE, syscall/sysret, SMEP, SMAP, NX, KASLR, and per-CPU data.
- **Memory manager:** Four-level page tables for v1, five-level paging as a v2 option, demand-zero pages, copy-on-write process cloning, memory-mapped files, page cache, NUMA-aware allocation in v2, kernel heap hardening, guard pages, and compressed memory as a future optimization.
- **Scheduler:** Preemptive priority scheduler with desktop latency classes, CPU affinity, power-aware core selection, deadline hooks for audio/compositor threads, starvation prevention, and scheduler tracing for Task Manager.
- **Process and syscall model:** Capability-oriented handles, job objects, process groups, threads, shared memory, async I/O ports, wait sets, signals/events, and a stable syscall ABI versioned separately from user-space libraries.
- **Driver model:** Signed loadable kernel modules for low-level drivers, user-mode driver framework for USB class devices and experimental hardware, crash-contained driver hosts, declarative device matching, and rollbackable driver packages.
- **Initial storage drivers:** NVMe, AHCI/SATA, USB mass storage, GPT/MBR partition parsing, EFI System Partition support, read/write ext4, read-only Btrfs v1 with writable snapshot support in v2, FAT32, and NTFS interoperability through a user-mode filesystem service after v1 stabilization.
- **Initial device drivers:** PS/2 and USB HID keyboard/mouse, basic USB xHCI, PCI/PCIe enumeration, ACPI power and battery, framebuffer/simpledrm-equivalent display, virtio block/network/input/gpu for VMs, Intel/AMD integrated graphics acceleration as a v2+ milestone, HDA audio as a v2 milestone, and Wi-Fi/Bluetooth through user-mode driver services or compatibility layers after wired networking is stable.
- **Networking:** Kernel packet path with Ethernet, ARP, IPv4, IPv6, UDP, TCP, DNS client hooks, firewall hooks, loopback, virtio-net, and e1000/e1000e-class drivers for early hardware/VM support. Wi-Fi support is a v2 goal because firmware, regulatory domains, and chipset diversity are high-risk.
- **Filesystem strategy:** Native ext4 support is the v1 install target. Aurelian Snapshot FS (`asfs`) is a future copy-on-write filesystem for restore points. NTFS interoperability focuses on external/shared drives and can ship as a sandboxed user-mode filesystem service rather than a kernel root filesystem.
- **POSIX and app compatibility:** Provide a POSIX-like libc and user-space ABI sufficient for shells, compilers, GUI apps, and ports. A Linux binary compatibility layer is future work and should not be required for v1 native apps.

### Display, compositor, and shell

- **Display server:** Native Aurelian compositor protocol for first-party apps, plus a Wayland-compatible protocol bridge for portable desktop apps. XWayland-style legacy support is a future compatibility milestone.
- **Compositor:** `lumad` uses the kernel graphics/input/display services directly in v1 with a software/framebuffer path first, then hardware-accelerated rendering as GPU drivers mature.
- **Window manager:** Integrated in `lumad`, supporting floating windows, snap layouts, virtual desktops, multi-monitor layout persistence, fractional scaling, HDR readiness, and capture portals.
- **Shell services:** `luma-shell` provides dock, Launchpad, notifications, quick settings, lock screen, session UI, and wallpaper.
- **UI toolkit:** Prism UI built on Qt 6 or GTK 4/libadwaita-derived components with original styling tokens. Web apps can use a matching design token package.

### Init, services, and configuration

- **Init system:** Original service manager named `aurinit`, responsible for dependency-ordered boot, service supervision, timers, journals, user sessions, crash restart policy, and recovery targets.
- **Service layout:** Core services are separated into boot, hardware, session, update, security, telemetry, package, network, and recovery domains.
- **Configuration storage:** Human-readable layered configuration under `/etc/aurelian`, user settings under XDG config directories, and dconf/KConfig-style schemas for desktop apps.
- **Logging:** systemd-journald for structured local logs, app logs through a privacy-preserving diagnostics API, and optional user-approved crash uploads.

### Package and app model

- **System packages:** Immutable or semi-immutable base OS image composed by an image builder from signed packages.
- **Desktop apps:** Sandboxed app bundles using Flatpak-like portals for camera, microphone, location, files, clipboard, screen capture, notifications, and background execution.
- **Developer packages:** Traditional repository packages available in power-user mode through `aurpkg` command-line tooling.
- **Software Center:** Uses signed repository metadata, app permissions display, staged rollout flags, and update channels.

### Core repositories/modules

- `aurelian-build`: ISO, image, signing, SBOM, and release pipeline.
- `aurelian-kernel`: original x86-64 kernel, boot ABI, scheduler, memory manager, drivers, kernel tests, and syscall interface.
- `aurelian-base`: package manifests, init presets, filesystems, compatibility services, and base policies.
- `luma-compositor`: Wayland compositor, window management, capture portals, display settings.
- `luma-shell`: dock, Launchpad, tray, notifications, lock screen, quick settings.
- `prism-ui`: design tokens, widgets, icons, animation curves, accessibility styles.
- `aurelian-installer`: live installer, partitioner, encryption flow, user creation, OEM mode.
- `aurelian-settings`: searchable settings panels and device management UI.
- `aurelian-store`: package center, update UX, repository client, app permission pages.
- `aurelian-updater`: atomic updates, rollback, delta downloads, policy engine.
- `aurelian-recovery`: recovery environment, restore UI, diagnostics, boot repair.
- `aurelian-apps`: native apps such as files, text editor, calculator, media viewer, PDF viewer.
- `aurelian-security`: elevation broker, sandbox portals, firewall UI, privacy dashboard.
- `aurelian-docs`: user docs, developer docs, hardware certification, release notes.

## Installer and boot flow

### Boot flow

1. Firmware loads ISO boot path through UEFI; optional legacy BIOS path uses ISOLINUX/SYSLINUX or GRUB BIOS modules.
2. Shim or signed GRUB validates bootloader components when Secure Boot is enabled.
3. Bootloader presents `Try Aurelian`, `Install Aurelian`, `Safe graphics`, `Memory test`, and `Recovery tools`.
4. Kernel and initramfs load with hardware detection, live root discovery, GPU mode setup, and optional safe graphics fallback.
5. Live environment starts `aurelian-live-session` and launches the installer if `Install Aurelian` was selected.
6. Installer writes target partitions, installs base image, configures bootloader, creates users, applies encryption, and stages first-boot setup.
7. First boot validates installation, expands machine-specific state, starts OOBE, and offers updates/drivers.

### Graphical installer

The installer is a Prism UI app named `Aurelian Setup` with these pages:

1. Language selection with locale preview.
2. Keyboard layout selection with test box.
3. Internet/offline choice with Wi-Fi join flow and offline package explanation.
4. Edition selection: Home, Pro, Developer, and Minimal. Editions are package profiles, not separate codebases.
5. Disk partitioning: guided erase, install alongside, manual partitioning, and advanced storage.
6. Partition preview with explicit destructive-action warnings.
7. Encryption option using LUKS2 full disk encryption, TPM2 unlock where supported, and recovery key export/print.
8. User creation with local account by default, optional online sync account in a future milestone.
9. Hostname selection and device purpose.
10. Summary review before install.
11. Progress screen with friendly tips, hardware checks, and installation log drawer.
12. Completion page with reboot, continue live session, or view log.

### First-boot welcome flow

1. Welcome and language confirmation.
2. Accessibility quick setup: screen reader, contrast, text size, reduced motion.
3. Network connection or continue offline.
4. Privacy choices: diagnostics level, location, advertising ID disabled by design, background app permissions.
5. Theme choice: light, dark, automatic, high contrast.
6. Account finalization: password, recovery hints, optional password manager vault setup.
7. Driver and firmware scan with recommended actions.
8. Restore from backup or start fresh.
9. Short desktop tour: Launchpad, Quick Settings, Software Center, Recovery.

## Default apps

### Native v1 apps

- **Files:** Tabs, quick access, network, archives, metadata, removable-drive safety, file operation queue.
- **Browser shell:** Minimal WebKit/Chromium-embedded browser for onboarding, captive portals, documentation, and web apps. A full third-party browser can be offered through Software Center.
- **Terminal:** Tabs, profiles, command palette, SSH bookmarks, quake mode in power-user mode.
- **Notes:** Plain text and Markdown editor with autosave and session restore.
- **Calculator:** Standard, scientific, programmer, unit conversion.
- **Media:** Audio/video playback through GStreamer or libmpv with playlists and hardware decode.
- **Photos:** Image viewer with crop, rotate, metadata, slideshow, and simple annotations.
- **Archives:** ZIP, tar, 7z, gzip, xz, zstd, rar extraction where legally supported.
- **Capture:** Screenshots, region/window/fullscreen capture, timed capture, screen recording.
- **System Monitor:** Processes, performance graphs, disks, startup, services, network.
- **Settings:** Searchable system settings app.
- **Software Center:** Apps, updates, permissions, release notes.
- **Backup:** Local, network, and external drive backup profiles.
- **Vault:** Password manager using OS keyring and encrypted local database.
- **Connect:** Bluetooth, Wi-Fi, VPN, mobile hotspot, and casting controls.
- **Docs:** PDF viewer and help/documentation portal.

### Optional bundled apps

- Full web browser, office suite, email client, chat client, code editor, enhanced PDF reader, music player, video conferencing tool, note-taking app, and cloud storage connectors. These should be selectable during install or offered after first boot to reduce ISO size.

## Security and privacy

### Boot and disk security

- Secure Boot support through signed bootloader, kernel, initramfs, and kernel modules.
- Measured boot with TPM event logs as a v2 goal.
- Full disk encryption through LUKS2, optional TPM2-assisted unlock, and mandatory user-held recovery key.
- ISO integrity through SHA-256/SHA-512 checksums, detached signatures, and reproducible build attestations.

### Permissions and isolation

- User account control through an elevation broker named `Gatekeeper`, using polkit/sudo policies with clear consent dialogs and command details.
- Sandboxed third-party apps by default with file, camera, microphone, location, clipboard, screen capture, notification, and background permissions mediated by portals.
- Privacy Dashboard lists recent permission use, allows per-app revocation, and blocks sensitive APIs for untrusted apps.
- Clipboard history excludes password fields and private windows through portal hints.
- Firewall enabled by default with simple profiles and advanced rule editor in power-user mode.

### Diagnostics and telemetry

- Local diagnostics always available; network telemetry is opt-in.
- Crash reports show data before upload and redact usernames, paths, tokens, environment variables, and document snippets.
- Update success/failure metrics can be submitted anonymously only after consent.

## Update and recovery system

### Update framework

- Atomic base OS updates with A/B deployments or snapshot-backed deployments.
- Delta downloads to reduce bandwidth.
- Signed metadata and packages with rollback protection.
- Channels: stable, beta, developer. Stable receives staged rollouts and automatic rollback on boot failure.
- App updates are independent from base OS updates so browser/media/security apps can update quickly.

### Recovery

- Recovery environment bootable from ISO, installed recovery partition, and bootloader menu.
- Safe mode disables third-party services, GPU acceleration, shell extensions, and startup apps.
- Restore points before updates and driver changes.
- Boot repair can reinstall bootloader, regenerate initramfs, unlock encrypted disks, and repair EFI entries.
- Diagnostics suite covers memory, storage SMART/NVMe health, battery, network, display, audio, and logs.
- Crash recovery detects repeated failed boots and offers rollback, safe graphics, or log export.

## ISO build and packaging plan

### Sample ISO file structure

```text
/AURELIAN-OS-1.0-x86_64.iso
├── EFI/
│   └── BOOT/
│       ├── BOOTX64.EFI
│       ├── grubx64.efi
│       └── mmx64.efi
├── boot/
│   ├── grub/
│   │   ├── grub.cfg
│   │   └── themes/aurelian/
│   ├── isolinux/                 # optional legacy BIOS
│   ├── vmlinuz
│   ├── initramfs.img
│   └── memtest.img
├── live/
│   ├── filesystem.squashfs
│   ├── filesystem.manifest
│   └── filesystem.size
├── pool/
│   ├── base/
│   ├── desktop/
│   ├── drivers/
│   └── optional/
├── installer/
│   ├── aurelian-installer
│   ├── edition-profiles.json
│   └── tips.json
├── recovery/
│   ├── recovery.squashfs
│   └── tools.manifest
├── docs/
│   ├── release-notes.html
│   ├── license-notices.html
│   └── install-guide.html
├── signatures/
│   ├── SHA256SUMS
│   ├── SHA256SUMS.sig
│   └── sbom.spdx.json
└── .disk/
    ├── info
    └── base_installable
```

### Build pipeline

1. Resolve package manifests and lock dependency versions.
2. Build kernel, initramfs, base packages, desktop packages, native apps, and installer.
3. Run unit tests, integration tests, static analysis, license scans, and SBOM generation.
4. Compose root filesystem image and live filesystem.
5. Build recovery image and installer package pool.
6. Generate bootloader configs for UEFI and optional BIOS.
7. Sign bootloader, kernel, modules, repositories, manifests, and ISO checksums.
8. Produce hybrid ISO with xorriso and USB image metadata.
9. Boot-test ISO automatically in QEMU/KVM for UEFI, secure boot path, safe graphics, offline install, encrypted install, and upgrade rollback.
10. Publish artifacts to release storage with checksums, signatures, SBOM, provenance attestations, and release notes.

## Hardware support strategy

- Track public AMD, Intel, ACPI, PCI, USB, NVMe, SATA, virtio, and UEFI specifications and implement drivers in the original Aurelion kernel.
- Prioritize VM and reference PC drivers first, then expand through a hardware enablement program. Include firmware packages that are legally redistributable; expose missing firmware clearly in Driver Manager.
- Certify core VM targets: VirtualBox, VMware, QEMU/KVM, Hyper-V as a future target.
- Provide guest integration packages: display resize, clipboard sharing, shared folders, time sync, pointer integration.
- Use `aur-netd` for Ethernet, Wi-Fi orchestration when drivers exist, VPN profiles, DHCP, DNS, captive portal detection, and mobile broadband as a later plugin.
- Use `aur-btd` for Bluetooth host policy after USB and radio drivers mature, with audio routing exposed through the native audio server.
- Use `aur-printd` with IPP Everywhere for driverless printing and a sandboxed scanner service for network scanning.
- Maintain a hardware report page with CPU, GPU, RAM, disks, firmware mode, secure boot state, TPM, battery health, network adapters, audio devices, cameras, and driver status.

## Quality assurance and testing plan

### Automated tests

- Unit tests for shell components, settings panels, installer logic, package manager, updater, and sandbox permission broker.
- Compositor protocol tests for window management, scaling, input, screenshots, and multi-monitor behavior.
- Installer integration tests using loopback disks for erase, alongside, manual, encrypted, and offline installs.
- Update tests for successful update, interrupted update, failed boot rollback, app update, repository metadata expiry, and channel switching.
- Accessibility tests for keyboard navigation, screen reader labels, contrast ratios, text scaling, and reduced motion.
- Performance benchmarks for cold boot, login, app launch, search indexing, file copy, suspend/resume, memory pressure, and animation frame pacing.

### Manual certification

- Bare-metal smoke tests on Intel laptop, AMD laptop, desktop with discrete GPU, low-memory PC, HiDPI display, printer, Bluetooth headset, and external monitor dock.
- VM smoke tests on VirtualBox, VMware, and QEMU/KVM.
- Security review for installer encryption, elevation prompts, sandbox escapes, update signatures, and crash report redaction.
- UX review with consumer and power-user personas.

## Milestone roadmap

### v1: Installable polished desktop

- UEFI bootable ISO, optional BIOS boot if schedule allows.
- Original Aurelion kernel base, ext4 install target, and snapshot-capable restore design.
- Luma Shell with dock, Launchpad, notifications, Quick Settings, file manager, settings, terminal, task manager, and core apps.
- Offline graphical installer with guided/manual partitioning, user creation, hostname, edition profiles, and install summary.
- LUKS2 encryption option.
- Software Center with signed repositories and app installs.
- Basic atomic/snapshot update and rollback.
- Privacy Dashboard v1 and elevation broker.
- VM integration packages and hardware report.

### v2: Complete desktop platform

- Secure Boot production signing path and measured boot.
- A/B system deployments with automatic health checks and rollback.
- Full sandboxed app permission model and app review metadata.
- Remote desktop/screen sharing.
- Advanced restore points and cloud/network backup targets.
- Per-workspace personalization, richer window snapping, improved screen recorder.
- Battery health analytics and per-app power recommendations.
- Driver Manager v2 with firmware updates and proprietary driver workflows where legally permitted.
- Stable/beta/developer channel infrastructure with staged rollout controls.

### v1+v2 complete edition

The full-feature edition combines all v1 and v2 capabilities into one target product: secure boot, encryption, atomic A/B updates, snapshot rollback, complete Prism UI shell, sandboxed app platform, remote desktop, advanced backups, driver workflows, staged channels, accessibility suite, privacy dashboard, full diagnostics, and polished native app set.

### v3+ future milestones

- Experimental original kernel research track.
- ARM64 builds.
- Enterprise device management.
- Cloud identity and sync account.
- HDR production workflow and color management UI.
- Gaming compatibility layer profiles.
- OEM recovery and factory provisioning tools.

## Risks, tradeoffs, and limitations

- A fully original kernel sharply narrows v1 hardware support compared with a Linux-based OS; v1 should certify a small matrix of VMs and reference PCs before broad consumer claims.
- Driver development, security hardening, filesystems, power management, graphics, Wi-Fi, Bluetooth, and printers become the largest schedule risks.
- Secure Boot production support requires key management, signing infrastructure, revocation planning, and potentially third-party review.
- Proprietary GPU/Wi-Fi drivers and codecs are limited by redistribution rights and regional patent concerns.
- Blur, shadows, and animations must degrade gracefully on low-end GPUs and in VMs.
- Atomic updates are safer but increase disk usage; ext4 users may get less complete rollback than Btrfs/A/B deployments.
- Sandboxed app ecosystems require developer tooling, documentation, review processes, and portal completeness.
- NTFS interoperability should focus on compatibility with external/shared drives, not using NTFS as the primary Linux root filesystem.
- Telemetry must remain opt-in to preserve trust, which reduces automatic product analytics.
