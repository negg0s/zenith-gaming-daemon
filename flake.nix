{
  description = "Ultra-low latency Linux gaming optimizer daemon (C++20)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages.default = pkgs.stdenv.mkDerivation rec {
          pname = "zenith-gaming-daemon";
          version = "1.2.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            pipewire
            libnotify
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
          ];

          doCheck = true;

          meta = with pkgs.lib; {
            description = "Ultra-low latency Linux gaming optimizer daemon (C++20)";
            homepage = "https://github.com/NEGG0/zenith-gaming-daemon";
            license = licenses.mit;
            platforms = platforms.linux;
            maintainers = [ ];
            mainProgram = "zenith";
          };
        };

        apps.default = flake-utils.lib.mkApp {
          drv = self.packages.${system}.default;
          name = "zenithctl";
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            cmake
            gcc13
            clang-tools
            pipewire
            lcov
          ];
        };
      }) // {
        nixosModules.default = { config, lib, pkgs, ... }:
          with lib;
          let
            cfg = config.services.zenith;
          in
          {
            options.services.zenith = {
              enable = mkEnableOption "Zenith ultra-low latency gaming optimizer daemon";
              package = mkOption {
                type = types.package;
                default = self.packages.${pkgs.system}.default;
                description = "Zenith package to install";
              };
            };

            config = mkIf cfg.enable {
              environment.systemPackages = [ cfg.package ];
              systemd.packages = [ cfg.package ];
              systemd.services.zenith = {
                wantedBy = [ "multi-user.target" ];
              };
            };
          };
      };
}
