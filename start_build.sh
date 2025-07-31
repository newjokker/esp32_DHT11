#!/bin/bash

pio run --target clean
pio run --target upload
pio run --target uploadfs --upload-port /dev/cu.wchusbserial5A7A0249031


