Name:           gmqcc
Version:        0.2.9
Release:        1%{?dist}
Summary:        Improved Quake C Compiler
License:        MIT
URL:            http://graphitemaster.github.io/gmqcc/
Source0:        https://github.com/graphitemaster/%{name}/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz

%description
Modern written-from-scratch compiler for the QuakeC language with
support for many common features found in other QC compilers.

%package -n qcvm
Summary:        Standalone QuakeC VM binary executor

%description -n qcvm
Executor for QuakeC VM binary files created using a QC compiler such
as gmqcc or fteqcc. It provides a small set of builtin functions, and
by default executes the main function if there is one. Some options
useful for debugging are available as well.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install
%make_install PREFIX=%{_prefix}

%check
make check

%files
%doc LICENSE README AUTHORS CHANGES
%doc %{_mandir}/man1/gmqcc.1.gz
%{_bindir}/gmqcc

%files -n qcvm
%doc LICENSE README AUTHORS CHANGES
%doc %{_mandir}/man1/qcvm.1.gz
%{_bindir}/qcvm

%changelog
* Sat Jul 27 2013 Igor Gnatenko <i.gnatenko.brain@gmail.com> - 0.2.9-1
- Initial release
