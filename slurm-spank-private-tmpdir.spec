Summary: Slurm SPANK plugin for job private tmpdir
Name: slurm-spank-private-tmpdir
Version: 0.0.1
Release: 1
License: GPL
Group: System Environment/Base
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: slurm-devel
Requires: slurm

%description
Slurm SPANK plugin that uses file system namespaces to create private
temporary directories for each job.

%prep
%setup -q

%build
make all

%install
install -d %{buildroot}%{_libdir}/slurm
install -d %{buildroot}%{_sysconfdir}/slurm/plugstack.conf.d
install -m 755 private-tmpdir.so %{buildroot}%{_libdir}/slurm/
install -m 644 plugstack.conf \
    %{buildroot}%{_sysconfdir}/slurm/plugstack.conf.d/private-tmpdir.conf

%clean
rm -rf %{buildroot}

%files
%doc README LICENSE
%defattr(-,root,root,-)
%{_libdir}/slurm/private-tmpdir.so
%config %{_sysconfdir}/slurm/plugstack.conf.d/private-tmpdir.conf

%changelog
* Mon Feb 16 2015 Pär Lindfors <paran@nsc.liu.se> - 0.0.1-1
- Initial RPM packaging
