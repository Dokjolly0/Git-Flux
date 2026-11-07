Name:           git-flux
Version:        0.1.0
Release:        1%{?dist}
Summary:        Git Flux — Modern local-first desktop Git client

License:        MIT
URL:            https://github.com/dokjolly/GitFlux
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.25
BuildRequires:  ninja-build
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  sqlite-devel

Requires:       git
Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative

%description
Git Flux is a high-performance, modern local-first Git client engineered in C++23
and Qt Quick 6. It features blazing fast branch management, visual commit tree
navigation, side-by-side diff viewers with rich syntax highlighting, integrated
conflict resolution, stash management, and multi-repository workspaces.

%prep
%autosetup

%build
%cmake -G Ninja \
    -DGF_BUILD_DESKTOP=ON \
    -DGF_BUILD_TESTING=OFF \
    -DGF_BUILD_BENCHMARKS=OFF
%cmake_build

%install
%cmake_install

%files
%{_bindir}/git_flux
%{_datadir}/applications/git-flux.desktop
%{_datadir}/icons/hicolor/256x256/apps/git-flux.png
%{_datadir}/pixmaps/git-flux.png
%{_datadir}/metainfo/com.gitflux.GitFlux.metainfo.xml

%changelog
* Sat Nov 07 2026 GitFlux Team <support@gitflux.org> - 0.1.0-1
- Initial release
