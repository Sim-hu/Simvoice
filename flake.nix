{
  description = "高性能読み上げ Discord Bot (VOICEVOX Core + DPP)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        voicevoxCoreDir = "/opt/voicevox_core";

        tts-bot = pkgs.stdenv.mkDerivation {
          pname = "tts-bot";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            dpp
            openssl
            zlib
            libopus
            sqlite
            spdlog
          ];

          cmakeFlags = [
            "-DENABLE_VOICEVOX=OFF"
            "-DBUILD_TESTS=OFF"
          ];

          meta = {
            description = "高性能読み上げ Discord Bot";
            license = pkgs.lib.licenses.mit;
            mainProgram = "tts-bot";
          };
        };

        tts-bot-voicevox = tts-bot.overrideAttrs (old: {
          pname = "tts-bot-voicevox";

          cmakeFlags = [
            "-DENABLE_VOICEVOX=ON"
            "-DVOICEVOX_CORE_DIR=${voicevoxCoreDir}/c_api"
            "-DBUILD_TESTS=OFF"
          ];

          postFixup = ''
            patchelf --add-rpath ${voicevoxCoreDir}/c_api/lib $out/bin/tts-bot
            patchelf --add-rpath ${voicevoxCoreDir}/onnxruntime/lib $out/bin/tts-bot
          '';
        });
      in
      {
        packages = {
          default = tts-bot;
          voicevox = tts-bot-voicevox;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ tts-bot ];

          packages = with pkgs; [
            gtest
            gbenchmark
            clang-tools
            valgrind
            gdb
          ];

          shellHook = ''
            export LD_LIBRARY_PATH="${voicevoxCoreDir}/onnxruntime/lib:${voicevoxCoreDir}/c_api/lib:''${LD_LIBRARY_PATH:-}"
            export VOICEVOX_CORE_DIR="${voicevoxCoreDir}"
          '';
        };
      }
    ) // {
      nixosModules.default = { config, lib, pkgs, ... }:
        let
          cfg = config.services.tts-bot;
        in
        {
          options.services.tts-bot = {
            enable = lib.mkEnableOption "TTS Discord Bot";

            package = lib.mkOption {
              type = lib.types.package;
              default = self.packages.${pkgs.system}.voicevox;
              description = "tts-bot パッケージ";
            };

            voicevoxCoreDir = lib.mkOption {
              type = lib.types.path;
              default = "/opt/voicevox_core";
              description = "VOICEVOX Core のインストールパス";
            };

            tokenFile = lib.mkOption {
              type = lib.types.path;
              description = "Discord Bot トークンファイル (sops-nix / agenix 等で管理)";
            };

            dataDir = lib.mkOption {
              type = lib.types.path;
              default = "/var/lib/tts-bot";
              description = "データディレクトリ (SQLite DB 等)";
            };

            extraEnv = lib.mkOption {
              type = lib.types.attrsOf lib.types.str;
              default = {};
              description = "追加の環境変数";
            };
          };

          config = lib.mkIf cfg.enable {
            systemd.services.tts-bot = {
              description = "TTS Discord Bot";
              after = [ "network-online.target" ];
              wants = [ "network-online.target" ];
              wantedBy = [ "multi-user.target" ];

              environment = {
                VOICEVOX_CORE_DIR = toString cfg.voicevoxCoreDir;
                LD_LIBRARY_PATH = lib.concatStringsSep ":" [
                  "${cfg.voicevoxCoreDir}/c_api/lib"
                  "${cfg.voicevoxCoreDir}/onnxruntime/lib"
                ];
              } // cfg.extraEnv;

              serviceConfig = {
                ExecStart = "${cfg.package}/bin/tts-bot";
                Restart = "always";
                RestartSec = 5;
                DynamicUser = true;
                StateDirectory = "tts-bot";
                WorkingDirectory = cfg.dataDir;
                LoadCredential = "discord-token:${cfg.tokenFile}";
                MemoryMax = "1G";
                NoNewPrivileges = true;
                ProtectSystem = "strict";
                ProtectHome = true;
                ReadOnlyPaths = [ (toString cfg.voicevoxCoreDir) ];
                ReadWritePaths = [ cfg.dataDir ];
              };
            };
          };
        };
    };
}
