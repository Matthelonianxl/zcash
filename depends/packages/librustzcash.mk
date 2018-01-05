package=librustzcash
$(package)_version=0.1
$(package)_download_path=https://github.com/zcash/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=bb691f4da72ac46ce6e26ad98c42747500d1605498e760866fe86b7235df7efd
$(package)_git_commit=0b862c9d5245ade620aeab5fc90f38206b2d9435
$(package)_dependencies=rust $(rust_crates)
$(package)_patches=cargo.config

define $(package)_preprocess_cmds
  mkdir .cargo && \
  cat $($(package)_patch_dir)/cargo.config | sed 's|CRATE_REGISTRY|$(host_prefix)/$(CRATE_REGISTRY)|' > .cargo/config
endef

define $(package)_build_cmds
  cargo build --frozen --release
endef

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/lib/ && \
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp target/release/librustzcash.a $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp include/librustzcash.h $($(package)_staging_dir)$(host_prefix)/include/
endef
