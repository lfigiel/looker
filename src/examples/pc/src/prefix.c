    looker_exit_t err;

    if (argc < 2)
    {
        printf("Error: No serial port provided\n");
        return 1;
    }

    serial_init(argv[1]);

    printf("looker init ... ");


	if ((err = looker_connect(LOOKER_SSID, LOOKER_PASS, DOMAIN)))
    {
        printf("Error: %d\n", err);
        return 1;
    }

    printf("done\n");

