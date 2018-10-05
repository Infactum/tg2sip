[![Build status](https://ci.appveyor.com/api/projects/status/4dnph34apvfy6vft?svg=true)](https://ci.appveyor.com/project/Infactum/tg2sip)

# TG2SIP

TG2SIP is a Telegram<->SIP voice gateway. It can be used to forward incoming telegram calls to your SIP PBX or make SIP->Telegram calls.

## Requirements

Your SIP PBX should be comaptible with `L16@48000` or `OPUS@48000` voice codec.

## Usage

1. [Download](https://ci.appveyor.com/api/projects/Infactum/tg2sip/artifacts/tg2sip.zip?branch=master) prebuild version from CI or compile it yourself.
2. Obtain `api_id` and `api_hash` tokens from [this](https://my.telegram.org) page and put them in `settings.ini` file.
3. Login into telegram with `gen_db` app
4. Set SIP server settings in `settings.ini`
5. Run `tg2sip`

SIP->Telegram calls can be done using 3 extension types:

1. `tg#[\s\d]+` for calls by username
2. `\+[\d]+` for calls by phone number
3. `[\d]+` for calls by telegram ID. Only known IDs allowed by telegram API.

## Donate

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=755FZWPRC9YGL&lc=US&item_name=TG2SIP&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted)

[Yandex.Money](https://yasobe.ru/na/tg2sip)

**BTC** 39wNzvtcyRrTKmq5DjcUfGTixnGVSf8qLg  
**BCH** qqgwg0g96sayht4lzxc89ky7mkdxfyj7jcl5m8qfps  
**ETH** 0x72B8cb476b2c85b1170Ae2cdFB243B17680290b4  
**ETC** 0x9C7d6CD9F9E0584e65f8aD20e1d2Ced947a55207  
**LTC** MFyBRJTnHqXharzH7D3FYeEhAJuywMRfMd  
