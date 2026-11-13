{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.services.ikeback;
in
{
  options.services.ikeback = {
    enable = lib.mkEnableOption "Ikeback - computer monitoring and control software";

    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.ikeback-unwrapped;
    };

    dataDir = lib.mkOption {
      type = lib.types.path;
      default = "/var/lib/ikeback";
    };

    publicKey = lib.mkOption {
      description = "Named public key stored under /etc/desktop/keys/public/<name>/key";

      type = lib.types.submodule {
        options = {
          name = lib.mkOption {
            type = lib.types.str;
            default = "test";
          };

          value = lib.mkOption {
            type = lib.types.lines;
            default = ''
              -----BEGIN PUBLIC KEY-----
              ...
              -----END PUBLIC KEY-----
            '';
          };
        };
      };
    };
  };

  config = lib.mkIf cfg.enable {
    users.groups.ikeback = { };

    environment.systemPackages = [ cfg.package ];

    environment.etc."desktop/keys/public/${cfg.publicKey.name}/key".text = cfg.publicKey.value;

    systemd.tmpfiles.rules = [ "d ${cfg.dataDir} 0750 root ikeback - -" ];

    systemd.services.ikeback-service = {
      description = "Ikeback Service";

      documentation = [ "man:ikeback-service(1)" ];

      after = [
        "network-online.target"
        "dbus.service"
        "systemd-logind.service"
      ];

      wants = [
        "network-online.target"
      ];

      requires = [
        "dbus.service"
        "systemd-logind.service"
      ];

      wantedBy = [ "multi-user.target" ];

      serviceConfig = {
        ExecStart = "${cfg.package}/bin/ikeback-service";
        Type = "simple";
        Restart = "always";
        RestartSec = 0;
      };

      startLimitIntervalSec = 60;
      startLimitBurst = 10;
    };

    networking.firewall = {
      allowedTCPPorts = [
        11100
        11200
        11300
      ];
      allowedUDPPorts = [
        11100
        11200
        11300
      ];
    };

    services.dbus.packages = [ cfg.package ];

    systemd.services.ikeback-auth-helper = {
      description = "Ikeback Authentication Helper";

      requires = [ "dbus.service" ];
      after = [ "dbus.service" ];

      serviceConfig = {
        Type = "dbus";
        BusName = "io.ikeback.AuthHelper";
        ExecStart = "${cfg.package}/bin/ikeback-auth-helper";
        NoNewPrivileges = true;
        PrivateTmp = true;
        ProtectKernelTunables = true;
        ProtectKernelModules = true;
        ProtectControlGroups = true;
        RestrictSUIDSGID = true;
        LockPersonality = true;
      };
    };
  };
}
