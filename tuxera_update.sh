#!/bin/bash

# ================= NO CHANGE NEEDED BELOW! =====================

usage() {
    usagestr=$(cat <<EOF
Usage: tuxera_update.sh [OPTION...]

  This script only assembles the kernel headers package
  (kheaders.tar.bz2) by default. Use -a to invoke Autobuild.

  --source-dir SRCDIR [--output-dir OUTDIR]

      Specify kernel source and optionally, kernel build output
      directories. This is necessary if kernel headers package assembly
      is performed.

  --no-excludes

      Do not exclude *.c, *.o, *.S, arch/*/boot. Only use if the build
      fails (some of the excluded files are needed). This significantly
      grows the headers package size.

  --user USER [--pass PASSWD]
      If remote connectivity is needed (-a or --latest), username and
      password are required. If missing, they will be read from stdin.

      NOTE: Using --pass can be dangerous as local users see it in 'ps'.

  --use-cache [--cache-dir CACHEDIR] [--latest] [--max-cache-entries N]
      Obtain modules from local cache if kernel dependencies are not
      modified. You must provide --source-dir (optionally --output-dir) to
      use the cache. Modules are relinked on cache hit, ensure the
      toolchain is in PATH. If -a is specified, Autobuild is invoked
      on a cache miss, and the cache is updated. Otherwise, kernel headers
      assembly is performed for later, manual build.

      You can specify a different cache directory with --cache-dir. The
      default directory is \$PWD/.tuxera_update_cache

      The --latest option will ensure that the release found in cache is
      the latest version available on Autobuild servers. This needs remote
      connectivity. If a new version is found and -a is specified,
      Autobuild is invoked and the cache is updated.

      The optional --max-cache-entries can be used to limit the maximum
      cache size to N recently used entries. The default is 10.

  -a [--target TARGET] [MODE] [OPTIONS]

      Start Autobuild against TARGET. If target was not specified, uses
      the default target. Use '--target list' to show available targets.

      Autobuild starts with fresh .tar.bz2 assembly by default, but in MODE
      you can optionally specify another operating mode:

        --use-package PACKAGE
          Start Autobuild by uploading a pre-built .tar.bz2 file PACKAGE.

        --use-remote-package PACKAGE
          Start Autobuild with a previously uploaded .tar.bz2 file PACKAGE.

      The following extra options are supported:

        --ignore-cert
          If up/download fails due to certificate issues, this option can
          be used to disable verification.

        --use-curl
        --use-wget
          Force the use of 'curl' or 'wget' for remote communication. Due
          to 'wget' limitations, 'sftp' is required with 'wget'.

        --version SOFTWARE=VERSION[,...]
          Example: --version NTFS=3012.4.2,EXFAT=3012.4.9
          Specify the version of software component(s) to build.

  -u, --upgrade

      Upgrade online to the latest script version, obtained from
      $upgrade_url

  -h, --help

      Prints this help.

  -v

      Only print the version of this script and exit.

  --verbose

      Print more debug information.

  Tuxera Autobuild Team
  autobuild-support@tuxera.com
EOF
)

  if [ -n "$long_help" ] ; then
      echo "$usagestr"
      echo
  else
      echo "For long help, use -h. Options:"
      echo "$usagestr" | grep "^ *-"
  fi
  exit 1
}

build_package() {
    if [ -z "$1" ] ; then
        echo "You must specify --source-dir for headers package assembly."
        usage
    fi

    LINK_DIR=$(mktemp -d)

    if [ $? -ne 0 ] ; then
        echo "mktemp failed. Unable to continue."
        exit 1
    fi

    KERNEL_LINK="${LINK_DIR}"/kernel
    OUTPUT_LINK="${LINK_DIR}"/output

    ln -sf "$(readlink -f "$1")" "${KERNEL_LINK}" && \
        if [ -n "$2" ]; then ln -sf "$(readlink -f "$2")" "${OUTPUT_LINK}"; fi

    if [ $? -ne 0 ] ; then
        echo "Symlinking (ln -s) failed. Unable to continue."
        rm -rf "${LINK_DIR}"
        exit 1
    fi

    if [ "$1" = "$2" ] ; then
        echo "  WARNING: You should not use the same --source-dir and --output-dir."
        echo "           Will not use --output-dir. Please see the user manual."
        rm "$OUTPUT_LINK"
    fi

    if test ! -e "${KERNEL_LINK}/COPYING"; then
        echo "  ERROR: Kernel source code directory is invalid (no COPYING found).";
        echo "         To fix it, set the --source-dir parameter correctly.";

        rm -rf "${LINK_DIR}"
        exit 1
    fi

    if test ! -e "${KERNEL_LINK}/include/config/auto.conf" -a ! -e "${OUTPUT_LINK}/include/config/auto.conf"; then
            echo "  ERROR: Invalid kernel configuration:";
            echo "         include/config/auto.conf is missing.";
            echo "         To fix it run 'make oldconfig && make modules_prepare' and ";
            echo "         'make sure ARCH= and CROSS_COMPILE= are correctly set and exported.";

        rm -rf "${LINK_DIR}"
        exit 1
    fi

    if test ! -e "${KERNEL_LINK}/Module.symvers" -a ! -e "${OUTPUT_LINK}/Module.symvers"; then
            echo "  ERROR: Invalid kernel configuration:";
            echo "         Module.symvers is missing.";
            echo "         To fix it run 'make oldconfig && make modules_prepare && make' and ";
            echo "         'make sure ARCH= and CROSS_COMPILE= are correctly set and exported.";

        rm -rf "${LINK_DIR}"
        exit 1
    fi

    echo "Generating list of files to include..."
    INCLUDEFILE=$(mktemp)

    if [ $? -ne 0 ] ; then
        echo "mktemp failed. Unable to continue."
        rm -rf "${LINK_DIR}"
        exit 1
    fi

    SEARCHPATHS="${KERNEL_LINK}/include ${KERNEL_LINK}/arch ${KERNEL_LINK}/scripts"
    if [ -L "${OUTPUT_LINK}" ] ; then
        SEARCHPATHS="${SEARCHPATHS} ${OUTPUT_LINK}/include ${OUTPUT_LINK}/arch ${OUTPUT_LINK}/scripts"
    fi

    for P in ${SEARCHPATHS}; do
        if [ ! -e ${P} ]; then continue; fi
        if [ -n "$no_excludes" ] ; then
            find -L ${P} ! -type l >> "${INCLUDEFILE}"
        else
            find -L ${P} \
            \( ! -type l -a ! -name \*.c -a ! -name \*.o -a ! -name \*.S -a ! -path \*/arch/\*/boot/\* -a ! -path \*/.svn/\* -a ! -path \*/.git/\* \) \
            >> ${INCLUDEFILE}
        fi
    done

    echo ${KERNEL_LINK}/Makefile >> ${INCLUDEFILE}
    if [ -e "${OUTPUT_LINK}/Makefile" ]; then echo "${OUTPUT_LINK}/Makefile" >> ${INCLUDEFILE}; fi
    if [ -e "${KERNEL_LINK}/Module.symvers" ]; then echo "${KERNEL_LINK}/Module.symvers" >> ${INCLUDEFILE}; fi
    if [ -e "${OUTPUT_LINK}/Module.symvers" ]; then echo "${OUTPUT_LINK}/Module.symvers" >> ${INCLUDEFILE}; fi
    if [ -e "${KERNEL_LINK}/.config" ]; then echo "${KERNEL_LINK}/.config" >> ${INCLUDEFILE}; fi
    if [ -e "${OUTPUT_LINK}/.config" ]; then echo "${OUTPUT_LINK}/.config" >> ${INCLUDEFILE}; fi

    echo "Packing kernel headers ..."
    tar cjf "${3}" --dereference --no-recursion --files-from "${INCLUDEFILE}"

    if [ $? -ne 0 ] ; then
        echo "'tar cjf ${3} ...' failed. I will now exit."
        rm -rf "${LINK_DIR}"
        exit 1
    fi

    rm ${INCLUDEFILE}
    rm -rf "${LINK_DIR}"
    echo "Headers package assembly succeeded. You could now use --use-package ${3}."
}

upload_package_curl() {
    echo "Uploading the following package:"
    ls -lh "${1}"

    reply=$($curl -F "file=@${1}" https://${server}/upload.php)

    if [ $? -ne 0 ] ; then
        echo "curl failed. Unable to continue. Check connectivity and username/password."
        exit 1
    fi

    status=$(echo "$reply" | head -n 1)

    if [ "$status" != "OK" ] ; then
        echo "Upload failed. Unable to continue."
        exit 1
    fi

    remote_package=$(echo "$reply" | head -n 2 | tail -n 1)

    echo "Upload succeeded. You could now use --use-remote-package ${remote_package}."
}

upload_package_sftp() {
    echo "Downloading SFTP key..."

    keyfile=$(mktemp)

    if [ $? -ne 0 ] ; then
        echo "mktemp failed. Unable to continue."
        exit 1
    fi

    $wget -O "$keyfile" https://${server}/id.php

    if [ $? -ne 0 ] ; then
        echo "Failed to download SFTP key. Unable to continue."
        echo "Check connectivity and username/password."
        exit 1
    fi

    chmod 400 "$keyfile"
    
    echo "Uploading kheaders package:"
    ls -lh ${1}

    remote_package="$(basename ${1})"
    sftp_path="kernels/${remote_package}"

    printf "put \"${1}\" \"${sftp_path}\"\nchmod 0770 \"${sftp_path}\"" | \
        sftp -oStrictHostKeyChecking=no -oIdentityFile="$keyfile" -b- "${username}@${server}"

    if [ $? -ne 0 ] ; then
        rm -f "$keyfile"
        echo "Upload failed. Unable to continue."
        exit 1
    fi

    rm -f "$keyfile"
    echo "Upload succeeded. You could now use --use-remote-package ${1}."
}

calc_header_checksums() {
    DEPENDENCY=$(mktemp)

    if [ $? -ne 0 ] ; then
        echo "mktemp failed. Unable to continue."
        return 1
    fi

    egrep '^#.*".*\.h".*' "$1"/*.i | awk -F '"' '{print $2}' | sort | uniq >> $DEPENDENCY

    if [ $? -ne 0 ] ; then
        echo "Failed to extract dependencies."
        rm -f "${DEPENDENCY}"
        return 1
    fi

    rm -f "$2"
    dirlen=$(readlink -f "${source_dir}" | wc -c)

    while read line; do 
        echo $line | grep ^include > /dev/null 2>&1

        # Common Headers
        if [ $? -ne 0 ]; then
            echo "$line" | grep ^"$(readlink -f "${source_dir}")" > /dev/null 2>&1

            if [ $? -ne 0 ]; then
                continue
            fi

            path_end=$(echo $line | tail -c +$dirlen)
            sum=$(md5sum "${line}" | cut -d ' ' -f 1 2>/dev/null)

            if [ $? -ne 0 ] ; then
                    echo "Failed to get checksum for file: ${linkfile}"
                    echo "Unable to continue."
                    rm -f "${DEPENDENCY}"
                    return 1
            fi

            echo "${sum} source/${path_end}" >> "$2"

        # Generated Headers
        else
            sum=$(md5sum "${kernel}/${line}" | cut -d ' ' -f 1 2>/dev/null)

            if [ $? -ne 0 ] ; then
                echo "Failed to get checksum for file: ${kernel}/${line}"
                echo "Unable to continue."
                rm -f "${DEPENDENCY}"
                return 1
            fi

            echo "${sum} generated/${line}" >> "$2"
        fi
    done < ${DEPENDENCY}

    rm "${DEPENDENCY}"
    return 0
}

gen_header_checksums() {
    if [ -f "${cache_dir}/pkgtmp/dependency_mod/env" ] ; then
        . "${cache_dir}/pkgtmp/dependency_mod/env"
    else
        echo "No build environment found, unable to produce header checksums."
        return 1
    fi

    if [ -n "${CROSS_COMPILE}" ] ; then
        command -v ${CROSS_COMPILE}gcc > /dev/null 2>&1

        if [ $? -ne 0 ] ; then
            echo "${CROSS_COMPILE}gcc not found. Do you have the cross compiler in PATH?"
            return 1
        fi
    fi

    make -C "$kernel" ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE $CUST_KENV M="${cache_dir}/pkgtmp/dependency_mod" depmod.i > $dbgdev
    
    if [ $? -ne 0 ] ; then
        echo "Compilation failed. Unable to compute header dependency tree."
        return 1
    fi

    calc_header_checksums "${cache_dir}/pkgtmp/dependency_mod" "$1"
    return $?
}

check_symvers() {
    searchdir="$1"
    symvers="$2"

    searchvers=$(mktemp)
    symvers_parsed=$(mktemp)

    sort $(find "$searchdir" -name \*.mod.c) | uniq | \
        egrep "{ 0x[[:xdigit:]]{8}," | awk '{print $2,$3}' | tr -d ',\"' > $searchvers

    awk '{print $1,$2}' "$symvers" > "$symvers_parsed"

    sort "$symvers_parsed" "$searchvers" | uniq -d | diff "$searchvers" - > /dev/null
    ret=$?

    rm -f "$symvers_parsed" "$searchvers"

    return $ret
}

destroy_cache_entry() {
    echo "Destroying cache entry: ${1}"
    rm "${cache_dir}/${pkg}".{pkg,md5sum,target,pkgname}
}

lookup_cache() {
    if [ ! -f "${kernel}/Module.symvers" ] ; then
        echo "${kernel}/Module.symvers does not exist. Unable to lookup cache."
        return 1
    fi

    cachefiles=$(cd "${cache_dir}"; ls -t *.pkg 2>/dev/null)

    if [ $? -ne 0 ] ; then
        echo "Can't find any cache files."
        return 1
    fi

    for pkg in ${cachefiles} ; do
        pkg=$(basename "$pkg" .pkg)
        echo -n "Cache lookup: ${pkg}.pkg ... "

        if [ ! -f "${cache_dir}/${pkg}.target" -o $(cat "${cache_dir}/${pkg}.target") != "$target" ] ; then
            echo "miss (different target)"
            continue
        fi

        if [ ! -f "${cache_dir}/${pkg}.md5sum" -o ! -f "${cache_dir}/${pkg}.pkgname" ] ; then
            echo "md5sum or pkgname file missing for ${cache_dir}/${pkg}, unable to validate"
            continue
        fi

        pkgname=$(cat "${cache_dir}/${pkg}.pkgname")

        if [ -n "$check_latest" -a "$pkgname" != "$latest_pkg" ] ; then
            echo "Old cache entry: new version available"
            [ -z "$autobuild" ] || destroy_cache_entry "old version"
            continue
        fi

        rm -rf "${cache_dir}/pkgtmp"
        mkdir "${cache_dir}/pkgtmp"
        tar xf "${cache_dir}/${pkg}.pkg" --strip-components=1 -C "${cache_dir}/pkgtmp"

        check_symvers "${cache_dir}/pkgtmp" "${kernel}/Module.symvers"
        if [ $? -ne 0 ] ; then
            echo "miss (kernel symbol CRCs differ)"
            rm -rf "${cache_dir}/pkgtmp"
            continue
        fi

        tmpsums=$(mktemp)
        gen_header_checksums "$tmpsums"
        if [ $? -ne 0 ] ; then
            echo "checksum calculation failed"
            rm -rf "${cache_dir}/pkgtmp"
            rm -f "$tmpsums"
            continue
        fi

        diff "$tmpsums" "${cache_dir}/${pkg}.md5sum" > /dev/null
        if [ $? -ne 0 ] ; then
            rm -rf "${cache_dir}/pkgtmp"
            rm -f "$tmpsums"
            echo "miss (header checksums differ)"
            continue
        fi

        rm -f "$tmpsums"

        echo "Cache hit! Relinking modules..."

        if [ -z "$(find "${cache_dir}/pkgtmp/" -name \*.ko)" ] ; then
            require_relink="yes"
        fi

        for driver_obj in "${cache_dir}/pkgtmp/"*/objects ; do
            driver=$(dirname "$driver_obj")

            if [ ! -f "${driver}/objects/Makefile.autobuild" ] ; then
                echo "Old cache entry: no Makefile.autobuild found"
                destroy_cache_entry "created by an old version"
                rm -rf "${cache_dir}/pkgtmp"
                continue 2
            fi

            rm -f "${driver}/objects/Kbuild"
            mv "${driver}/objects/Makefile.autobuild" "${driver}/objects/Makefile"

            make -C "$kernel" ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE $CUST_KENV \
                M="${driver}/objects" modules

            if [ $? -ne 0 ] ; then
                echo "Kernel module relinking failed - this usually shouldn't happen."
                echo "Refusing to use this cache entry!"
                rm -rf "${cache_dir}/pkgtmp"
                continue 2
            fi

            echo "Stripping debug information..."

            for mod in "${driver}"/objects/*.ko ; do
                "${CROSS_COMPILE}strip" --strip-debug "${mod}" 1>/dev/null 2>/dev/null
                cp "${mod}" "${driver}/kernel-module/"
            done

            relinked="yes"
        done

        if [ -n "$require_relink" -a -z "$relinked" ] ; then
            echo "Old cache entry: no objects for relinking found"
            destroy_cache_entry "created by an old version"
            rm -rf "${cache_dir}/pkgtmp"
            continue
        fi

        echo "Packaging ${pkgname}..."
        pkgcontents=$(tar tf "${cache_dir}/${pkg}.pkg")
        origname=$(echo "${pkgcontents}" | head -n 1 | awk -F '/' '{print $1}')
        rm -rf "${cache_dir}/${origname}"
        mv "${cache_dir}/pkgtmp" "${cache_dir}/${origname}"
        tar czf "$(pwd)/${pkgname}" -C "${cache_dir}" "${origname}/"
        rm -rf "${cache_dir}/${origname}"

        touch "${cache_dir}/${pkg}.pkg"
        return 0
    done

    echo "No cache hits."
    return 1
}

do_remote_build() {
    echo "Starting remote build against target ${target}..."

    if [ "$http_client" = "wget" ] ; then
        reply=$($wget --post-data="terminal=1&filename=${remote_package}&target-config=${target}&tags=${tags}&extraargs=${extraargs}&use-cache=${using_cache}&script-version=${script_version}&cache-lookup-time=${cache_lookup_time}&start-build=1" -O - https://${server})
    else
        reply=$($curl -d terminal=1 -d filename="$remote_package" -d target-config="$target" -d tags="$tags" -d extraargs="$extraargs" -d use-cache="$using_cache" -d script-version="$script_version" -d cache-lookup-time="$cache_lookup_time" -d start-build=1 https://${server})
    fi

    if [ $? -ne 0 ] ; then
        echo "${http_client} failed. Unable to start build."
        echo "Check connectivity and username/password."
        exit 1
    fi
    
    status=$(echo "$reply" | head -n 1)

    if [ "$status" != "OK" ] ; then
        echo "Starting the build failed. Unable to continue."
        echo "The server reported:"
        echo "$status"
        exit 1
    fi
    
    build_id=$(echo "$reply" | head -n 2 | tail -n 1)
    
    echo "Build started, id ${build_id}"
    echo "Polling for completion every 10 seconds..."
    
    statusurl="https://${server}/builds/${build_id}/.status"

    while [ 1 ]
    do
        if [ "$http_client" = "wget" ] ; then
            reply=$($wget_quiet -O - "$statusurl")
        else
            reply=$($curl_quiet "$statusurl")
        fi
        
        if [ $? -ne 0 ] ; then
            echo "Not finished yet; waiting..."
            sleep 10
            continue
        fi
        
        break
    done
    
    echo "Build finished."
    
    status=$(echo "$reply" | head -n 1)
    
    if [ "$status" != "OK" ] ; then
        echo "Build failed. Cannot download package."
        echo "Tuxera has been notified of this failure."
        exit 1
    fi
    
    filename=$(echo "$reply" | head -2 | tail -1)
    fileurl="https://${server}/builds/${build_id}/${filename}"
    
    echo "Downloading ${filename} ..."
    
    if [ "$http_client" = "wget" ] ; then
        $wget -O "$filename" "$fileurl"
    else
        $curl -o "$filename" "$fileurl"
    fi
    
    if [ $? -ne 0 ] ; then
        echo "Failed. You can still try to download using the link in the e-mail that was sent."
        exit 1
    fi
    
    echo "Download finished."

    if [ -n "$use_cache" ] ; then
        echo "Updating cache..."

        pkgprefix="$(date +%Y-%m-%d-%H-%M-%S)-$(head -c 8 /dev/urandom | md5sum | head -c 4)"
        cp "$filename" "${cache_dir}/${pkgprefix}.pkg"
        echo "$target" > "${cache_dir}/${pkgprefix}.target"
        echo "$filename" > "${cache_dir}/${pkgprefix}.pkgname"

        mkdir "${cache_dir}/pkgtmp"
        tar xf "${cache_dir}/${pkgprefix}.pkg" --strip-components=1 -C "${cache_dir}/pkgtmp"

        gen_header_checksums "${cache_dir}/${pkgprefix}.md5sum"
        if [ $? -ne 0 ] ; then
            echo "Updating cache failed."
            rm -rf "${cache_dir}/${pkgprefix}.pkg"
            rm -f "${cache_dir}/${pkgprefix}.target"
        fi

        rm -rf "${cache_dir}/pkgtmp"

        cachefiles=$(cd "${cache_dir}"; ls -t *.pkg 2>/dev/null | tail -n +$((${max_cache_entries}+1)))

        for pkg in ${cachefiles} ; do
            pkg=$(basename "$pkg" .pkg)
            destroy_cache_entry "enforcing cache size limit"
        done
    fi
}

list_targets() {
    echo "Connecting..."

    if [ "$http_client" = "wget" ] ; then
        reply=$($wget -O - https://${server}/targets.php)
    else
        reply=$($curl https://${server}/targets.php)
    fi

    if [ $? -ne 0 ] ; then
        echo "Unable to list targets. Check connectivity and username/password."
        exit 1
    fi

    echo
    echo "Available targets for this user:"
    echo "$reply"
    echo
}

get_latest() {
    echo "Checking for latest release..."

    if [ "$http_client" = "wget" ] ; then
        latest_pkg=$($wget --post-data="target-config=${target}" -O - https://${server}/latest.php)
    else
        latest_pkg=$($curl -d target-config="$target" https://${server}/latest.php)
    fi

    if [ $? -ne 0 ] ; then
        echo "Unable to get latest release. Check connectivity and username/password."
        exit 1
    fi

    if [ "$latest_pkg" = "FAIL" -o -z "$latest_pkg" ] ; then
        echo "Unable to get latest release for this target."
        echo "Use '-a --target list' to get valid targets."
        exit 1
    fi

    echo "Latest release is ${latest_pkg}"
}

check_http_client() {
    while [ -z "$username" ] ; do
        echo -n "Please enter your username: "
        read username
    done

    while [ -z "$password" ] ; do
        oldstty=$(stty -g)
        echo -n "Please enter your password: "
        stty -echo
        read password
        stty "$oldstty"
        echo
    done

    curl_quiet="curl -f -u ${username}:${password}"
    wget="wget --user ${username} --password ${password}"
    wget_quiet=${wget}

    if [ -z "${verbose}" ] ; then
        curl_quiet=${curl_quiet}" -s"
        wget_quiet=${wget_quiet}" -q"
        wget=${wget}" -nv"
    fi
    
    if [ -n "$ignore_certificates" ] ; then
        curl_quiet=${curl_quiet}" -k"
        wget_quiet=${wget_quiet}" --no-check-certificate"
        wget=${wget}" --no-check-certificate"
    fi

    curl=${curl_quiet}" -S"

    if [ -n "$http_client" ] ; then
        echo "HTTP client forced to ${http_client}"
        return
    fi
    
    http_client="curl"
    echo -n "Checking for 'curl'... "
    command -v curl > /dev/null

    if [ $? -ne 0 ] ; then
        echo "no."
        http_client="wget"
        echo -n "Checking for 'wget'... "
        command -v wget > /dev/null
    fi

    if [ $? -ne 0 ] ; then
        echo "no. Unable to continue."
        exit 1
    fi

    echo "yes."
}

check_cmds() {
    echo -n "Checking for: "

    for c in $* ; do
        echo -n "$c "
        command -v $c > /dev/null

        if [ $? -ne 0 ] ; then
            echo "... no."
            echo "Unable to continue."
            exit 1
        fi
    done

    echo "... yes."
    return 0
}

check_autobuild_prerequisites() {
    check_cmds date stty mktemp chmod tail head md5sum basename

    if [ "$http_client" = "wget" ] ; then
        echo -n "Checking for 'sftp'... "

        command -v sftp > /dev/null

        if [ $? -ne 0 ] ; then
            echo "no. Unable to continue."
            exit 1
        fi

        echo "yes."
    fi

    if [ -n "$use_cache" ] ; then
        check_cmds egrep sed awk touch uniq sort tr diff make \
            dirname wc

        kernel="$source_dir"

        if [ -n "$output_dir" ] ; then
            kernel="$output_dir"
        fi
    fi
}

upgrade() {
    check_http_client
    check_cmds mktemp

    tmpscript=$(mktemp)

    if [ "$http_client" = "wget" ] ; then
        $wget -O "$tmpscript" "$upgrade_url"
    else
        $curl -o "$tmpscript" "$upgrade_url"
    fi

    if [ $? -ne 0 ] ; then
        echo "Upgrade failed."
        exit 1
    fi

    mv "$tmpscript" "$0"

    echo "Before upgrade: tuxera_update.sh version $script_version"
    echo "After upgrade: $(sh $0 -v)"
}

script_version="12.11.27"
cache_dir=".tuxera_update_cache"
server="autobuild.tuxera.com"
upgrade_url="https://$server/tuxera_update.sh"
dbgdev="/dev/null"
cache_lookup_time="none"
max_cache_entries=10

echo "tuxera_update.sh version $script_version"

if ! options=$(getopt -o pahuv -l target:,user:,pass:,use-package:,use-remote-package:,source-dir:,output-dir:,version:,cache-dir:,server:,extraargs:,max-cache-entries:,upgrade,help,ignore-cert,no-check-certificate,use-curl,use-wget,no-excludes,use-cache,verbose,latest -- "$@")
then
    usage
fi

eval set -- "$options"

while [ $# -gt 0 ]
do
    case $1 in
    -p) pkgonly="yes" ;;
    -a) autobuild="yes" ;;
    -v) exit 0 ;;
    --target) target="$2" ; shift;;
    --user) username="$2" ; shift;;
    --pass) password="$2" ; shift;;
    --use-package) local_package="$2" ; shift;;
    --use-remote-package) remote_package="$2" ; shift;;
    --source-dir) source_dir="$2" ; shift;;
    --output-dir) output_dir="$2" ; shift;;
    --ignore-cert) ignore_certificates="yes" ;;
    --no-check-certificate) ignore_certificates="yes" ;;
    --use-wget) http_client="wget" ;;
    --use-curl) http_client="curl" ;;
    --version) tags="$2" ; shift;;
    --extraargs) extraargs="$2" ; shift;;
    --no-excludes) no_excludes="yes" ;;
    --use-cache) use_cache="yes" ;;
    --cache-dir) cache_dir="$2" ; shift;;
    --server) server="$2" ; shift;;
    --help | -h) long_help="yes"; usage;;
    --upgrade | -u) do_upgrade="yes" ;;
    --verbose) verbose="yes"; dbgdev="/dev/stdout" ;;
    --latest) check_latest="yes" ;;
    --max-cache-entries) max_cache_entries="$2" ; shift;;
    (--) shift; break;;
    (-*) echo "$0: error - unrecognized option $1" 1>&2; usage;;
    (*) break;;
    esac
    shift
done

if [ -n "$do_upgrade" ] ; then
    upgrade
    exit 0
fi

if [ -z "$target" ] ; then
    target="default"
fi

check_cmds tar find grep readlink

if [ -n "$pkgonly" ] && [ -n "$autobuild" -o -n "$use_cache" ] ; then
    echo "You cannot specify -p with -a or --use-cache."
    usage
fi

if [ -n "$local_package" -a -n "$remote_package" ] ; then
    echo "You cannot specify both local and remote packages."
    usage
fi

if [ -n "$local_package" -o -n "$remote_package" ] && [ -n "$use_cache" ] ; then
    echo "You cannot specify --use-package/--use-remote-package with --use-cache."
    usage
fi

if [ -n "$use_cache" -a "$target" != "list" ] ; then
    check_autobuild_prerequisites

    mkdir -p "${cache_dir}"
    cache_dir=$(readlink -f "$cache_dir")

    if [ $(echo $cache_dir | wc -w) != "1" ] ; then
        echo "Linux build system does not support module paths with whitespace."
        exit 1
    fi

    if [ -z "$source_dir" ] ; then
        echo "You must specify kernel source (optionally output) dir to use the cache."
        usage
    fi

    if [ -d "$cache_dir" ] ; then
        if [ -n "$check_latest" ] ; then
            check_http_client
            get_latest
        fi

        cache_lookup_start=$(date '+%s')
        lookup_cache

        if [ $? -eq 0 ] ; then
            exit 0
        else
            cache_lookup_time=$(($(date '+%s') - $cache_lookup_start))

            if [ -n "$autobuild" ] ; then
                echo "Proceeding with remote build..."
            else
                echo "No cache hit found, assembling headers to kheaders.tar.bz2 for manual build..."
            fi
        fi
    else
        echo "Local cache does not exist (yet)."
    fi
fi

if [ -n "$autobuild" ] ; then
    [ -n "$use_cache" ] || check_autobuild_prerequisites
    [ -n "$check_latest" ] || check_http_client

    if [ "$target" = "list" ] ; then
        list_targets
        exit 0
    fi

    if [ -z "$local_package" -a -z "$remote_package" ] ; then
        local_package="kheaders_$(date +%Y-%m-%d-%H-%M-%S-$(head -c 8 /dev/urandom | md5sum | head -c 4)).tar.bz2"
        build_package "$source_dir" "$output_dir" "$local_package"
    fi

    if [ -z "$remote_package" ] ; then
        if [ "$http_client" = "wget" ] ; then
            upload_package_sftp "$local_package"
        else
            upload_package_curl "$local_package"
        fi
    fi

    using_cache=$use_cache
    if [ -z "$use_cache" ] ; then
        using_cache="no"
    fi

    do_remote_build "$remote_package"
    exit 0
fi

build_package "$source_dir" "$output_dir" "kheaders.tar.bz2"
exit 0
