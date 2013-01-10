# Contributor: matthiaskrgr <matthiaskrgr _strange_curverd_character_ freedroid D0T org>

pkgname=gmqcc-git
pkgver=20130110
pkgrel=1
pkgdesc="An Improved Quake C Compiler"
arch=('i686' 'x86_64')
depends=('glibc')
conflicts=('gmqcc')
provides=('gmqcc=0.2.4')
makedepends=('git')
url="https://github.com/graphitemaster/gmqcc.git"
license=('MIT')

_gitroot="git://github.com/graphitemaster/gmqcc.git"
_gitname="gmqcc"

build() {
	cd $srcdir
	msg "Connecting to the GIT server..."
	if [[ -d $srcdir/$_gitname ]] ; then
		cd $_gitname
		msg "Removing build files..."
		git clean -dfx
		msg "Updating..."
		git pull --no-tags
		msg "The local files are updated."
	else
		msg "Cloning..."
		git clone $_gitroot $_gitname --depth 1
		msg "Clone done."
	fi

	msg "Starting compilation..."
	cd "$srcdir"/"$_gitname"

	msg "Compiling..."
	make
}

check() {
	cd "$srcdir"/"$_gitname"
	make check
}

package() {
	cd "$srcdir"/"$_gitname"
	msg "Compiling and installing to pkgdir this time..."
	make install DESTDIR=$pkgdir PREFIX=/usr
	msg "Compiling done."

	install -D LICENSE ${pkgdir}/usr/share/licenses/gmqcc/LICENSE
}
