Name:           zenith-gaming-daemon
Version:        1.0.0
Release:        1%{?dist}
Summary:        Ultra-low latency Linux gaming optimizer daemon (C++20)

License:        MIT
URL:            https://github.com/NEGG0/zenith-gaming-daemon
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.22
BuildRequires:  gcc-c++ >= 12
BuildRequires:  systemd-rpm-macros
Requires:       glibc >= 2.35
Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

%description
Zenith is an ultra-low latency Linux gaming optimization daemon engineered in
modern C++20 for high-refresh competitive gaming on Arch Linux, Fedora, and SteamOS.
Features AMD 3D V-Cache pinning, cgroups v2 soft isolation, latency-bounded sysctl,
adaptive thermal-pressure throttling, and zero-drift journaled state rollback.

%prep
%autosetup

%build
%cmake -DCMAKE_BUILD_TYPE=Release
%cmake_build

%install
%cmake_install

%post
%systemd_post zenith.service

%preun
if [ $1 -eq 0 ]; then
    # Package uninstall: trigger emergency rollback
    if [ -x %{_bindir}/zenithctl ]; then
        %{_bindir}/zenithctl rollback || true
    fi
fi
%systemd_preun zenith.service

%postun
%systemd_postun_with_restart zenith.service

%files
%license LICENSE
%doc README.md docs/
%{_bindir}/zenith
%{_bindir}/zenithctl
%{_bindir}/zenith-run
%dir %{_sysconfdir}/zenith
%config(noreplace) %{_sysconfdir}/zenith/zenith.conf
%config(noreplace) %{_sysconfdir}/zenith/profiles.conf
%{_unitdir}/zenith.service

%changelog
* Tue Oct 20 2026 Emir (NEGG0) <rokmit599@gmail.com> - 1.0.0-1
- Initial production release v1.0.0 with full UX, TUI dashboard, and rollback guarantees.
