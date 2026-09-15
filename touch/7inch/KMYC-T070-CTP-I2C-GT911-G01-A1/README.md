# KMYC-T070-CTP-I2C-GT911-G01-A1

7-inch capacitive-touch product using a Goodix GT911. The driver polls the controller
at I2C address `0x5d` or `0x14`, reads its product and firmware identity, and reports up
to five contacts.

The controller's stored configuration is used; this driver does not overwrite touch
firmware or calibration data. GPIO selection and coordinate transforms belong to the
development-board adapter.
