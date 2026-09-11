Name:           proxor
Version:        %{?version}%{!?version:0}
Release:        %{?release}%{!?release:1}%{?dist}
Summary:        Qt proxy client for sing-box profiles
License:        GPL-3.0-or-later
URL:            https://github.com/Ogstra/proxor
Source0:        proxor-%{version}.tar.gz
BuildArch:       x86_64
BuildRequires:   cmake ninja-build gcc-c++ golang qt6-qtbase-devel qt6-qtsvg-devel yaml-cpp-devel zxing-cpp-devel protobuf-devel
Requires:        policycoreutils, libcap

%description
Qt client for sing-box profiles. TUN authorization is explicitly initiated by
the user and this package never grants capabilities in scriptlets.

%prep
%autosetup -n proxor-%{version}

%build
./libs/build_public_res.sh
GOOS=linux GOARCH=amd64 ./libs/build_go.sh
%cmake -GNinja -DQT_VERSION_MAJOR=6 -DCMAKE_BUILD_TYPE=Release -DNKR_PACKAGE=ON
%cmake_build

%install
DESTDIR="%{buildroot}" ./packaging/linux/stage-native-root.sh --gui build/proxor --core deployment/linux64/proxor_core --geodata deployment/public_res
install -Dpm0644 assets/linux/proxor.desktop %{buildroot}%{_datadir}/applications/proxor.desktop
install -Dpm0644 assets/res/public/proxor.png %{buildroot}%{_datadir}/icons/hicolor/256x256/apps/proxor.png

%files
%{_bindir}/proxor
%{_libdir}/proxor/proxor
%{_libdir}/proxor/proxor_core
%{_datadir}/proxor/geoip.dat
%{_datadir}/proxor/geosite.dat
%{_datadir}/proxor/geoip.db
%{_datadir}/proxor/geosite.db
%{_datadir}/applications/proxor.desktop
%{_datadir}/icons/hicolor/256x256/apps/proxor.png

%changelog
* Thu Sep 11 2026 Proxor maintainers <maintainers@ogstra.github.io> - %{version}-1
- Native Fedora package from verified source archive
