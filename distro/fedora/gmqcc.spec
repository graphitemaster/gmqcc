Name:           gmqcc
Version:        0.2.9
Release:        1%{?dist}
Summary:        Improved Quake C Compiler
License:        MIT
URL:            http://graphitemaster.github.io/gmqcc/
Source0:        https://github.com/graphitemaster/%{name}/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz
# Downstream patch. TODO: drop it in 0.3.0 release.
Patch0:         build_fix.patch

%description
Modern written-from-scratch compiler for the QuakeC language with
support for many common features found in other QC compilers.

%package -n qcvm
Summary:        Standalone QuakeC VM binary executor

%description -n qcvm
Executor for QuakeC VM binary files created using a QC compiler such
as gmqcc or fteqcc. It provides a small set of built-in functions, and
by default executes the main function if there is one. Some options
useful for debugging are available as well.

# TODO: add new package gmqpak after 0.3.0 release

%prep
%setup -q
%patch0 -p1
echo '#!/bin/sh' > ./configure
chmod +x ./configure 

%build
%configure
make %{?_smp_mflags}

%install
%make_install PREFIX=%{_prefix}

%check
make check

%files
%doc LICENSE README AUTHORS CHANGES TODO
%doc gmqcc.ini.example
%doc %{_mandir}/man1/gmqcc.1.gz
%{_bindir}/gmqcc

%files -n qcvm
%doc LICENSE README AUTHORS CHANGES TODO
%doc %{_mandir}/man1/qcvm.1.gz
%{_bindir}/qcvm

%changelog
* Sat Jul 27 2013 Igor Gnatenko <i.gnatenko.brain@gmail.com> - 0.2.9-1
- Initial release
