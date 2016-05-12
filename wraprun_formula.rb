class WraprunFormula < Formula
  homepage "https://github.com/olcf/wraprun"
  url "https://github.com/olcf/wraprun/archive/v0.2.3.tar.gz"

  supported_build_names /python2.7/, /python3/

  depends_on do
     [python_module_from_build_name, "python_yaml"]
  end

  concern for_version("dev") do
    included do
      url "none"

      module_commands do
        pe = "PE-"
        pe = "PrgEnv-" if cray_system?

        commands = [ "unload #{pe}gnu #{pe}pgi #{pe}cray #{pe}intel python" ]

        commands << "load #{pe}gnu" if build_name =~ /gnu/
        commands << "swap gcc gcc/#{$1}" if build_name =~ /gnu([\d\.]+)/

        commands << "load #{pe}pgi" if build_name =~ /pgi/
        commands << "swap pgi pgi/#{$1}" if build_name =~ /pgi([\d\.]+)/

        commands << "load #{pe}intel" if build_name =~ /intel/
        commands << "swap intel intel/#{$1}" if build_name =~ /intel([\d\.]+)/

        commands << "load #{pe}cray" if build_name =~ /cray/
        commands << "swap intel cray/#{$1}" if build_name =~ /cray([\d\.]+)/

				commands << "load #{python_module_from_build_name}"
				commands << "load python_setuptools"
				commands << "load python_yaml"

        commands << "load dynamic-link"
        commands << "load cmake3"
        commands << "load git"

        commands
      end

      def install
        module_list

				Dir.chdir "#{prefix}"
        system "rm -rf source"
        system "git clone https://github.com/olcf/wraprun.git source" unless Dir.exists?("source")
				Dir.chdir "#{prefix}/source"
        system "git checkout origin/development"
        # normal build from here:
        system "rm -rf build; mkdir build"
				Dir.chdir "build"
        system "cmake -DCMAKE_INSTALL_PREFIX=#{prefix} .."
        system "make"
        system "make install"
				Dir.chdir "#{prefix}/source/python"
				system_python "setup.py install --root=#{prefix} --prefix=''"
      end
    end
  end

  module_commands do
    pe = "PE-"
    pe = "PrgEnv-" if cray_system?

    commands = [ "unload #{pe}gnu #{pe}pgi #{pe}cray #{pe}intel python" ]

    commands << "load #{pe}gnu" if build_name =~ /gnu/
    commands << "swap gcc gcc/#{$1}" if build_name =~ /gnu([\d\.]+)/

    commands << "load #{pe}pgi" if build_name =~ /pgi/
    commands << "swap pgi pgi/#{$1}" if build_name =~ /pgi([\d\.]+)/

    commands << "load #{pe}intel" if build_name =~ /intel/
    commands << "swap intel intel/#{$1}" if build_name =~ /intel([\d\.]+)/

    commands << "load #{pe}cray" if build_name =~ /cray/
    commands << "swap intel cray/#{$1}" if build_name =~ /cray([\d\.]+)/

    commands << "load #{python_module_from_build_name}"
		commands << "load python_setuptools"
    commands << "load python_yaml"

    commands << "load dynamic-link"
    commands << "load cmake3"

    commands
  end

  def install
    module_list
    system "rm -rf build; mkdir build"
    Dir.chdir "build"
    system "cmake -DCMAKE_INSTALL_PREFIX=#{prefix} .."
    system "make"
    system "make install"
    Dir.chdir "#{prefix}/source/python"
    system_python "setup.py install --root=#{prefix} --prefix=''"
  end


  modulefile <<-MODULEFILE.strip_heredoc
    #%Module
    proc ModulesHelp { } {
       puts stderr "<%= @package.name %> <%= @package.version %>"
       puts stderr ""
    }
    # One line description
    module-whatis "<%= @package.name %> <%= @package.version %>"

    if { ! [ is-loaded python ] } {
      puts stderr "WARNING: Wraprun requires python."
      puts stderr "The default python module is being loaded by wraprun but it will not be removed when the wraprun module is unloaded."
      puts stderr "Manually load any python module version prior to wraprun to supress this message."
      puts stderr ""
      module load python
    }

    prereq python
    module load python_yaml
    prereq python_yaml
    module load dynamic-link

    setenv W_UNSET_PRELOAD 1 
    setenv W_IGNORE_SEGV 1
    setenv W_IGNORE_RETURN_CODE 1
    setenv W_IGNORE_ABRT 1
    setenv W_SIG_DFL 1

		<%= pe_with_python_build_list @package, @builds %>

    set PREFIX <%= @package.version_directory %>/$BUILD

		prepend-path PATH            $PREFIX/bin
		prepend-path LD_LIBRARY_PATH $PREFIX/lib
		prepend-path PYTHONPATH      $PREFIX/lib/$LIBDIR/site-packages

    # The libfmpich library is suffixed with the PE name, so we must extract it
    set compiler $env(PE_ENV)
    set compiler [string tolower $compiler]

    set libmpichf $env(MPICH_DIR)/lib/libfmpich_$compiler.so
    if { [file exists $libmpichf] == 0 } {
      set libmpichf $env(MPICH_DIR)/lib/libfmpich.so
      if { [file exists $libmpichf] == 0 } {
        puts stderr "Module failed, libfmpich.so not found!"
        exit [1]
      }
    }

    setenv WRAPRUN_PRELOAD $libmpichf:$PREFIX/lib/libsplit.so

  MODULEFILE
end

Smithy::ModuleFile.class_eval do
  def pe_with_python_build_list(package, builds, args={})
    output = ""
    Smithy::ModuleFile::compilers.each_with_index do |e,i|
      versions = Hash.new do |cvs,cv|
        cvs[cv] = Hash.new do |pvs,pv|
          pvs[pv] = Array.new
        end
      end

      comp_builds = builds.select{|b| b =~ e[:build_name_regex]}
      comp_builds.each do |b|
        b =~ e[:build_name_regex]
        cv = "#{$2}"
        b =~ /(python)([0-9\.]*)/
        pv = "#{$2}"
        versions[cv][pv] << b if module_is_available?(get_python_version_from_build_name(b))
      end

      if i == 0
        output << "if "
      else
        output << "} elseif "
      end

      output << "[ is-loaded #{e[:prg_env]} ] {\n"
      versions.each_with_index do |(cv, ps), ci|
        if ci == 0
          output << "  if "
        else
          output << "  } elseif "
        end
        output << "[ is-loaded #{e[:compiler_name]}/#{cv} ] {\n"
        ps.each_with_index do |(pv, bs), pi|
          if pi == 0
            output << "    if "
          else
            output << "    } elseif "
          end
          output << "[ is-loaded python/#{pv} ] {\n"
          if bs.length > 1
            output << "      puts stderr \"Warning: possible wraprun modulefile name collision.\"\n"
            output << "      puts stderr \"Please notify the system administrator.\"\n"
          end
          bs.each_with_index do |b, bi|
            output << "      set LIBDIR #{python_libdir(get_python_version_from_build_name(b))}\n"
            output << "      set BUILD  #{b}\n"
            if ((bi == bs.size - 1) and (pi == ps.size - 1))
              output << "    }\n"
            end
          end
          if pi == ps.size - 1
            output << "  }\n"
          end
        end
      end
    end
    output << "}"
    return output
  end
end
