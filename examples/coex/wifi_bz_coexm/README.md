# wifi_bz_coexm

Wi-Fi and BT/BLE/BZ coexistence validation demo. The project keeps the
multi-chip startup structure from `wifi_bluetooth` and provides the BL618DG
BLE throughput and RF-path helpers previously carried by `wifi_coexm`.

## BL618DG build

```text
make CHIP=bl618dg BOARD=bl618dgdk CPU_ID=ap
```

## BL618DG coex helpers

```text
setup_bt_path
spdt_pin <rf-switch-gpio>
wifi_coex_bt_spdt <0|1>
```

`setup_bt_path` calls the PHYRF-provided BT/BLE standalone-path API.
`spdt_pin` configures the selected GPIO as function 25. Even GPIO numbers use
the normal SPDT polarity and odd GPIO numbers use the inverted polarity.
`wifi_coex_bt_spdt` is supplied by the shared Wi-Fi coex CLI and is not
reimplemented in this demo.

The BL618DG configuration also enables `iperf` and `ble_tp_test` for Matrix A
and Matrix D throughput validation.
