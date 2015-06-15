class WraprunFormula < Formula
  homepage "https://github.com/olcf/wraprun"
  additional_software_roots [ config_value("lustre-software-root")[hostname] ]
  url "https://github.com/olcf/wraprun/archive/v0.0.0.tar.gz"

  module_commands do
    pe = "PE-"
    pe = "PrgEnv-" if cray_system?

    commands = [ "unload #{pe}gnu #{pe}pgi #{pe}cray #{pe}intel" ]

    commands << "load #{pe}gnu" if build_name =~ /gnu/
    commands << "swap gcc gcc/#{$1}" if build_name =~ /gnu([\d\.]+)/

    commands << "load #{pe}pgi" if build_name =~ /pgi/
    commands << "swap pgi pgi/#{$1}" if build_name =~ /pgi([\d\.]+)/

    commands << "load #{pe}intel" if build_name =~ /intel/
    commands << "swap intel intel/#{$1}" if build_name =~ /intel([\d\.]+)/

    commands << "load #{pe}cray" if build_name =~ /cray/
    commands << "swap intel cray/#{$1}" if build_name =~ /cray([\d\.]+)/

    commands << "load dynamic-link"
    commands << "load cmake"

    commands
  end

  def install
    module_list
    system "rm -rf build"
    system "mkdir build"
    system "cd build; cmake -DCMAKE_INSTALL_PREFIX=#{prefix} .."
    system "cd build; make"
    system "cd build; make install"
  end

  modulefile <<-MODULEFILE.strip_heredoc
    #%Module
    proc ModulesHelp { } {
       puts stderr "<%= @package.name %> <%= @package.version %>"
       puts stderr ""
    }
    # One line description
    module-whatis "<%= @package.name %> <%= @package.version %>"

    module load dynamic-link

    <% if @builds.size > 1 %>
    <%= module_build_list @package, @builds %>

    set PREFIX <%= @package.version_directory %>/$BUILD
    <% else %>
    set PREFIX <%= @package.prefix %>
    <% end %>

    set LUSTREPREFIX /lustre/atlas/sw/xk7/<%= @package.name %>/<%= @package.version %>/<%= @package.build_name %>

    prepend-path PATH             $PREFIX/bin
    prepend-path LD_LIBRARY_PATH  $PREFIX/lib

    prepend-path PATH             $LUSTREPREFIX/bin
    prepend-path LD_LIBRARY_PATH  $LUSTREPREFIX/lib

    # The libfmpich library is suffixed with the PE name, so we must extract it
    set compiler $env(PE_ENV)
    set compiler [string tolower $compiler]

    setenv WRAPRUN_PRELOAD $env(MPICH_DIR)/lib/libfmpich_$compiler.so:$LUSTREPREFIX/lib/libsplit.so
  MODULEFILE
end
