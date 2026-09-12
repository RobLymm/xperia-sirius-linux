# Battery percentage

The Z2 has no fuel gauge chip that mainline supports. Battery percentage here
comes from the PMIC's ADC reading the battery voltage, converted through an
open-circuit-voltage table.

Two patches, both against existing mainline drivers:

`0005-iio-adc-qcom-spmi-vadc-scale-vbat-sns.patch`
: report the VBAT_SNS channel as a properly scaled voltage rather than raw
  counts.

`0006-power-supply-generic-adc-battery-ocv-capacity.patch`
: let `generic-adc-battery` estimate capacity from an OCV table in the device
  tree, instead of only reporting voltage.

Both are candidates for upstream in their own right, and neither is specific
to this phone. Voltage-based capacity is inherently approximate: it moves
under load and recovers when idle.
