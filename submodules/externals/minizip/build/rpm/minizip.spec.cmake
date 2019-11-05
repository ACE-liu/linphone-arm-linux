# -*- rpm-spec -*-

%define _prefix    @CMAKE_INSTALL_PREFIX@
%define pkg_prefix @BC_PACKAGE_NAME_PREFIX@

%define build_number 1


Name:           @CPACK_PACKAGE_NAME@
Version:        @PROJECT_VERSION@
Release:        %{build_number}%{?dist}
Summary:        Minizip zlib contribution fork with latest bug fixes that supports PKWARE disk splitting, AES encryption, and IO buffering


Group:          Applications/Archiving
License:        zlib
URL:            https://github.com/nmoinvaz/minizip
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Minizip zlib contribution fork with latest bug fixes that supports PKWARE disk splitting, AES encryption, and IO buffering.


%package        devel
Summary:        Headers, libraries and docs for the minizip library
Group:          Development/Libraries

Requires:       %{name} = %{version}

%description    devel
Minizip zlib contribution fork with latest bug fixes that supports PKWARE disk splitting, AES encryption, and IO buffering.

This package contains header files and development libraries needed to
develop programs using the minizip library.


%if 0%{?rhel} && 0%{?rhel} <= 7
%global cmake_name cmake3
%define ctest_name ctest3
%else
%global cmake_name cmake
%define ctest_name ctest
%endif

# This is for debian builds where debug_package has to be manually specified, whereas in centos it does not
%define custom_debug_package %{!?_enable_debug_packages:%debug_package}%{?_enable_debug_package:%{nil}}
%custom_debug_package

%prep
%setup -n %{name}-%{version}

%build
%{expand:%%%cmake_name} . -DCMAKE_BUILD_TYPE=@CMAKE_BUILD_TYPE@ -DCMAKE_INSTALL_LIBDIR:PATH=%{_libdir} -DCMAKE_PREFIX_PATH:PATH=%{_prefix} -USE_AES=NO
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%check
%{ctest_name} -V %{?_smp_mflags}

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc LICENSE README.md
%{_libdir}/*.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/*.h
%{_libdir}/cmake/minizip/*.cmake
%{_libdir}/pkgconfig/*.pc

%changelog
* Tue Mar 13 2018 ronan.abhamon <ronan.abhamon@belledonne-communications.com>
- Initial RPM release.
